#include "hmnpch.h"
#include "SceneRenderer.h"

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>

#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Renderer/EnvironmentProbe.h"

namespace Hominem {

void SceneRenderer::SetImGuiCallbacks(std::function<void()> waitFn,
                                      std::function<void()> notifyFn)
{
    m_WaitImGui   = std::move(waitFn);
    m_NotifyImGui = std::move(notifyFn);
}

void SceneRenderer::Init()
{
    m_AutoExposure.Init(ComputeShader::Create("engine://Shaders/luminance.comp"));

    Renderer3D::InitForwardPlus();

    SetupPasses();

    m_ThresholdShader = Renderer3D::GetShaderLibrary()->Get("bloom_threshold");
    m_BlurShader      = Renderer3D::GetShaderLibrary()->Get("bloom_blur");
    m_CompositeShader = Renderer3D::GetShaderLibrary()->Get("composite");
}

void SceneRenderer::Shutdown()
{
    m_ThresholdShader.reset();
    m_BlurShader.reset();
    m_CompositeShader.reset();
}

void SceneRenderer::SetupPasses()
{
    // HDR has 2 color attachments: [0] = rendered scene, [1] = view-space normals + linear depth
    m_RenderGraph.AddFBO("hdr",        FramebufferFormat::RGBA16F, 1.0f, 2);
    m_RenderGraph.AddFBO("bloom",      FramebufferFormat::RGBA16F, 0.25f);
    m_RenderGraph.AddFBO("bloom_temp", FramebufferFormat::RGBA16F, 0.25f);

    m_RenderGraph.AddPass("scene",          [this](RenderGraph&, const RenderFrame& f) { GeometryPass(f);       });
    m_RenderGraph.AddPass("imgui",          [this](RenderGraph&, const RenderFrame&  ) { ImGuiPass();           });
    m_RenderGraph.AddPass("auto_exposure",  [this](RenderGraph&, const RenderFrame& f) { AutoExposurePass(f);   });
    m_RenderGraph.AddPass("bloom_threshold",[this](RenderGraph&, const RenderFrame& f) { BloomThresholdPass(f); });
    m_RenderGraph.AddPass("bloom_blur_h",   [this](RenderGraph&, const RenderFrame& f) { BloomBlurHPass(f);     });
    m_RenderGraph.AddPass("bloom_blur_v",   [this](RenderGraph&, const RenderFrame& f) { BloomBlurVPass(f);     });
    m_RenderGraph.AddPass("composite",      [this](RenderGraph&, const RenderFrame& f) { CompositePass(f);      });
}

void SceneRenderer::RenderScene(const RenderFrame& frame)
{
    m_RenderGraph.Execute(frame);
}

void SceneRenderer::GeometryPass(const RenderFrame& frame)
{
    // Bake before BeginScene so the result is visible from the next frame onward.
    if (frame.bakeEnvMap && frame.bakedEnvMapOut)
    {
        auto baked = EnvironmentProbe::Bake(frame.bakeCapPos, frame, frame.bakeResolution);
        frame.bakedEnvMapOut->map = std::move(baked);
        frame.bakedEnvMapOut->ready.store(true, std::memory_order_release);
    }

    if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;

    const auto hdr = m_RenderGraph.GetFBO("hdr");
    hdr->Bind();

    RenderCommand::SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
    RenderCommand::SetScissorEnabled(false);
    RenderCommand::SetClearColor(frame.clearColor);
    RenderCommand::Clear();

    RenderCommand::SetDepthTestEnabled(false);
    Renderer2D::BeginScene(frame.viewProjection2D);

    for (const auto&[transform, color, uvMin, uvMax, texture] : frame.quads)
        Renderer2D::PushQuad(transform, color, uvMin, uvMax, texture);

    Renderer2D::Flush();

    for (const auto& t : frame.texts)
        Renderer2D::DrawString(t.text, t.font, t.transform, t.color);

    Renderer2D::EndScene();

    RenderCommand::SetDepthTestEnabled(true);
    Renderer3D::BeginScene(frame);

    for (const auto& sm : frame.staticMeshes)
        Renderer3D::DrawStaticMesh(*sm.mesh, sm.transform);

    for (const auto& m : frame.meshes)
    {
        m.mesh->DispatchSkinning(m.bones);
        Renderer3D::DrawSkinnedMesh(*m.mesh, m.transform);
    }

    Renderer3D::EndScene();

    if (frame.debugLights && !frame.lights.empty())
        Renderer3D::DrawDebugLights(frame.lights);

    hdr->Unbind();
}

void SceneRenderer::ImGuiPass()
{
    if (m_WaitImGui) m_WaitImGui();

    m_RenderGraph.GetFBO("hdr")->Bind();
    ImGui_ImplOpenGL3_NewFrame();
    if (ImDrawData* drawData = ImGui::GetDrawData())
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    m_RenderGraph.GetFBO("hdr")->Unbind();

    if (m_NotifyImGui) m_NotifyImGui();
}

void SceneRenderer::AutoExposurePass(const RenderFrame& frame)
{
    if (!frame.toneMappingEnabled) return;
    auto hdr = m_RenderGraph.GetFBO("hdr");
    m_AutoExposure.Compute(hdr->GetColorAttachmentRendererID(),
                           frame.viewportWidth, frame.viewportHeight);
}

void SceneRenderer::BloomThresholdPass(const RenderFrame& frame)
{
    if (!frame.bloomEnabled) return;
    auto bloom = m_RenderGraph.GetFBO("bloom");
    bloom->Bind();
    auto& spec = bloom->GetSpecification();
    RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
    RenderCommand::SetClearColor({ 0.f, 0.f, 0.f, 1.f });
    RenderCommand::Clear();
    RenderCommand::SetDepthTestEnabled(false);
    RenderCommand::SetScissorEnabled(false);
    m_ThresholdShader->Bind();
    m_ThresholdShader->SetInt("u_HDR", 0);
    m_ThresholdShader->SetFloat("u_Threshold", frame.bloomThreshold);
    RenderCommand::BindTexture(0, m_RenderGraph.GetFBO("hdr")->GetColorAttachmentRendererID());
    RenderCommand::DrawFullscreenTriangle();
    bloom->Unbind();
}

void SceneRenderer::BloomBlurHPass(const RenderFrame& frame)
{
    if (!frame.bloomEnabled) return;
    auto dst = m_RenderGraph.GetFBO("bloom_temp");
    dst->Bind();
    auto& spec = dst->GetSpecification();
    RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
    RenderCommand::SetDepthTestEnabled(false);
    RenderCommand::SetScissorEnabled(false);
    m_BlurShader->Bind();
    m_BlurShader->SetInt("u_Src", 0);
    m_BlurShader->SetInt("u_Horizontal", 1);
    RenderCommand::BindTexture(0, m_RenderGraph.GetFBO("bloom")->GetColorAttachmentRendererID());
    RenderCommand::DrawFullscreenTriangle();
    dst->Unbind();
}

void SceneRenderer::BloomBlurVPass(const RenderFrame& frame)
{
    if (!frame.bloomEnabled) return;
    auto dst = m_RenderGraph.GetFBO("bloom");
    dst->Bind();
    auto& spec = dst->GetSpecification();
    RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
    RenderCommand::SetDepthTestEnabled(false);
    RenderCommand::SetScissorEnabled(false);
    m_BlurShader->Bind();
    m_BlurShader->SetInt("u_Src", 0);
    m_BlurShader->SetInt("u_Horizontal", 0);
    RenderCommand::BindTexture(0, m_RenderGraph.GetFBO("bloom_temp")->GetColorAttachmentRendererID());
    RenderCommand::DrawFullscreenTriangle();
    dst->Unbind();
}

void SceneRenderer::CompositePass(const RenderFrame& frame)
{
    if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;
    auto hdrFBO = m_RenderGraph.GetFBO("hdr");
    if (!hdrFBO) return;
    RenderCommand::SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
    RenderCommand::SetDepthTestEnabled(false);
    RenderCommand::SetScissorEnabled(false);
    m_CompositeShader->Bind();
    m_CompositeShader->SetInt("u_HDR",              0);
    m_CompositeShader->SetInt("u_Bloom",             1);
    m_CompositeShader->SetFloat("u_Exposure",        m_AutoExposure.GetExposure());
    m_CompositeShader->SetFloat("u_BloomStrength",   frame.bloomStrength);
    m_CompositeShader->SetInt("u_BloomEnabled",       frame.bloomEnabled       ? 1 : 0);
    m_CompositeShader->SetInt("u_ToneMappingEnabled", frame.toneMappingEnabled ? 1 : 0);
    RenderCommand::BindTexture(0, hdrFBO->GetColorAttachmentRendererID());
    RenderCommand::BindTexture(1, m_RenderGraph.GetFBO("bloom")->GetColorAttachmentRendererID());
    RenderCommand::DrawFullscreenTriangle();
    RenderCommand::SetDepthTestEnabled(true);
}

}
