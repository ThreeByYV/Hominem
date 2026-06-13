#include "hmnpch.h"
#include "SceneRenderer.h"
#include "Hominem/Core/Profiler.h"

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
    m_SkyboxShader    = Renderer3D::GetShaderLibrary()->Get("skybox");
    m_FireQuadShader  = Renderer3D::GetShaderLibrary()->Get("fire_quad");
    m_SmokeQuadShader = Renderer3D::GetShaderLibrary()->Get("smoke_quad");
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
    m_RenderGraph.AddFBO("bloom",      FramebufferFormat::RGBA8,   0.25f);
    m_RenderGraph.AddFBO("bloom_temp", FramebufferFormat::RGBA8,   0.25f);

    m_RenderGraph.AddPass("scene",          [this](RenderGraph&, const RenderFrame& f) { GeometryPass(f);       });
    m_RenderGraph.AddPass("auto_exposure",  [this](RenderGraph&, const RenderFrame& f) { AutoExposurePass(f);   });
    m_RenderGraph.AddPass("bloom_threshold",[this](RenderGraph&, const RenderFrame& f) { BloomThresholdPass(f); });
    m_RenderGraph.AddPass("bloom_blur_h",   [this](RenderGraph&, const RenderFrame& f) { BloomBlurHPass(f);     });
    m_RenderGraph.AddPass("bloom_blur_v",   [this](RenderGraph&, const RenderFrame& f) { BloomBlurVPass(f);     });
    m_RenderGraph.AddPass("composite",      [this](RenderGraph&, const RenderFrame& f) { CompositePass(f);      });
    m_RenderGraph.AddPass("imgui",          [this](RenderGraph&, const RenderFrame& f) { ImGuiPass(f);          });
}

void SceneRenderer::RenderScene(const RenderFrame& frame)
{
    HMN_PROFILE_FUNCTION();
    m_RenderGraph.SetRenderScale(frame.renderScale);
    m_RenderGraph.Execute(frame);
}

void SceneRenderer::GeometryPass(const RenderFrame& frame)
{
    HMN_PROFILE_FUNCTION();
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

    // Use the FBO's actual dimensions — may be smaller than viewport when renderScale < 1.
    const auto& hdrSpec = hdr->GetSpecification();
    RenderCommand::SetViewport(0, 0, hdrSpec.Width, hdrSpec.Height);
    RenderCommand::SetScissorEnabled(false);
    RenderCommand::SetClearColor(frame.clearColor);
    RenderCommand::Clear();

    // Paint the background before any content. Depth disabled so it fills
    // every pixel; the 3D pass later overwrites wherever geometry is drawn.
    if (m_SkyboxShader && frame.skybox && frame.skybox->GetRendererID() != 0)
    {
        RenderCommand::SetDepthTestEnabled(false);
        m_SkyboxShader->Bind();
        m_SkyboxShader->SetInt  ("u_Equirect",    0);
        m_SkyboxShader->SetMat4 ("u_InvViewProj", glm::inverse(frame.viewProjection3D));
        m_SkyboxShader->SetFloat3("u_CamPos",     frame.cameraWorldPos);
        m_SkyboxShader->SetFloat("u_Intensity",   frame.skyboxIntensity);
        RenderCommand::BindTexture(0, frame.skybox->GetRendererID());
        RenderCommand::DrawFullscreenTriangle();
    }

    RenderCommand::SetDepthTestEnabled(true);
    Renderer3D::BeginScene(frame);

    for (const auto& sm : frame.staticMeshes)
        Renderer3D::DrawStaticMesh(*sm.mesh, sm.transform);

    for (const auto& m : frame.meshes)
    {
        if (m.overrideShader) Renderer3D::SetOverrideShader(m.overrideShader);
        m.mesh->DispatchSkinning(m.bones);
        Renderer3D::DrawSkinnedMesh(*m.mesh, m.transform);
        if (m.overrideShader) Renderer3D::ClearOverrideShader();
    }

    Renderer3D::EndScene();

    if (frame.debugLights && !frame.lights.empty())
        Renderer3D::DrawDebugLights(frame.lights);

    // Procedural smoke quads — alpha-blended, drawn before the fire quads so the
    // fire glow shows through the smoke. Depth-tested but doesn't write depth.
    if (!frame.smokeQuads.empty() && m_SmokeQuadShader)
    {
        RenderCommand::SetDepthWriteEnabled(false);
        RenderCommand::SetBlendMode(BlendMode::Alpha);
        RenderCommand::SetCullFaceEnabled(false); // smoke quads are flat planes — visible from either side
        m_SmokeQuadShader->Bind();
        for (const auto& sq : frame.smokeQuads)
        {
            m_SmokeQuadShader->SetMat4  ("u_Model",      sq.transform);
            m_SmokeQuadShader->SetFloat3("u_ColorDark",  sq.colorDark);
            m_SmokeQuadShader->SetFloat3("u_ColorLit",   sq.colorLit);
            m_SmokeQuadShader->SetFloat ("u_Opacity",    sq.opacity);
            m_SmokeQuadShader->SetFloat ("u_ScrollSpeed",sq.scrollSpeed);
            m_SmokeQuadShader->SetFloat ("u_Time",       sq.time);
            m_SmokeQuadShader->SetFloat ("u_Seed",       sq.seed);
            RenderCommand::DrawUnitQuad();
        }
        RenderCommand::SetCullFaceEnabled(true);
        RenderCommand::SetDepthWriteEnabled(true);
    }

    // Procedural fire quads — additive, depth-tested against the scene (so they're
    // occluded by geometry) but don't write depth (so overlapping flames blend cleanly).
    {
        static bool s_LoggedFireState = false;
        if (!s_LoggedFireState && !frame.fireQuads.empty())
        {
            HMN_CORE_INFO("Fire: {} quad(s) queued, shader valid = {}",
                          frame.fireQuads.size(), m_FireQuadShader != nullptr);
            for (size_t i = 0; i < frame.fireQuads.size(); i++)
            {
                const auto& fq = frame.fireQuads[i];
                HMN_CORE_INFO("Fire[{}]: intensity={:.2f} scrollSpeed={:.2f} pos=({:.2f},{:.2f},{:.2f})",
                              i, fq.intensity, fq.scrollSpeed,
                              fq.transform[3][0], fq.transform[3][1], fq.transform[3][2]);
            }
            s_LoggedFireState = true;
        }
    }
    if (!frame.fireQuads.empty() && m_FireQuadShader)
    {
        RenderCommand::SetDepthWriteEnabled(false);
        RenderCommand::SetBlendMode(BlendMode::Additive);
        RenderCommand::SetCullFaceEnabled(false); // fire quads are flat planes — visible from either side
        m_FireQuadShader->Bind();
        for (const auto& fq : frame.fireQuads)
        {
            m_FireQuadShader->SetMat4  ("u_Model",      fq.transform);
            m_FireQuadShader->SetFloat3("u_ColorCore",  fq.colorCore);
            m_FireQuadShader->SetFloat3("u_ColorMid",   fq.colorMid);
            m_FireQuadShader->SetFloat3("u_ColorEdge",  fq.colorEdge);
            m_FireQuadShader->SetFloat ("u_Intensity",  fq.intensity);
            m_FireQuadShader->SetFloat ("u_ScrollSpeed",fq.scrollSpeed);
            m_FireQuadShader->SetFloat ("u_Time",       fq.time);
            m_FireQuadShader->SetFloat ("u_Seed",       fq.seed);
            RenderCommand::DrawUnitQuad();
        }
        RenderCommand::SetBlendMode(BlendMode::Alpha);
        RenderCommand::SetCullFaceEnabled(true);
        RenderCommand::SetDepthWriteEnabled(true);
    }

    // 2D overlay pass last for screen-space content (fades, captions, HUD) must
    // composite on top of 3D geometry, not be overdrawn by it.
    RenderCommand::SetDepthTestEnabled(false);
    Renderer2D::BeginScene(frame.viewProjection2D);

    for (const auto&[transform, color, uvMin, uvMax, texture] : frame.quads)
        Renderer2D::PushQuad(transform, color, uvMin, uvMax, texture);

    Renderer2D::Flush();

    for (const auto& t : frame.texts)
        Renderer2D::DrawStringMultiline(t.text, t.font, t.transform, t.color, t.colorRight);

    Renderer2D::EndScene();

    hdr->Unbind();
}

void SceneRenderer::ImGuiPass(const RenderFrame& frame)
{
    if (m_WaitImGui) m_WaitImGui();

    // Render directly into the back buffer (no FBO bound) at full window resolution.
    // This must run after composite so the viewport is correct and UI is unaffected
    // by render scale.
    RenderCommand::SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
    ImGui_ImplOpenGL3_NewFrame();
    if (ImDrawData* drawData = ImGui::GetDrawData())
        ImGui_ImplOpenGL3_RenderDrawData(drawData);

    if (m_NotifyImGui) m_NotifyImGui();
}

void SceneRenderer::AutoExposurePass(const RenderFrame& frame)
{
    HMN_PROFILE_FUNCTION();
    if (!frame.toneMappingEnabled) return;
    auto hdr = m_RenderGraph.GetFBO("hdr");
    const auto& spec = hdr->GetSpecification();
    m_AutoExposure.Compute(hdr->GetColorAttachmentRendererID(),
                           spec.Width, spec.Height);
}

void SceneRenderer::BloomThresholdPass(const RenderFrame& frame)
{
    HMN_PROFILE_FUNCTION();
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
    HMN_PROFILE_FUNCTION();
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
    HMN_PROFILE_FUNCTION();
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
    HMN_PROFILE_FUNCTION();

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
