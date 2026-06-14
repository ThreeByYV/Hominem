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

    m_RenderGraph.AddPass("scene",           PipelineState::DepthTestWriteCull(), [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { GeometryPass(f, cmd);       });
    m_RenderGraph.AddPass("auto_exposure",   PipelineState::NoDepthNoCull(),      [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { AutoExposurePass(f, cmd);   });
    m_RenderGraph.AddPass("bloom_threshold", PipelineState::NoDepthNoCull(),      [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { BloomThresholdPass(f, cmd); });
    m_RenderGraph.AddPass("bloom_blur_h",    PipelineState::NoDepthNoCull(),      [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { BloomBlurHPass(f, cmd);     });
    m_RenderGraph.AddPass("bloom_blur_v",    PipelineState::NoDepthNoCull(),      [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { BloomBlurVPass(f, cmd);     });
    m_RenderGraph.AddPass("composite",       PipelineState::NoDepthNoCull(),      [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { CompositePass(f, cmd);      });
    m_RenderGraph.AddPass("imgui",           PipelineState::AlphaBlendNoDepth(),  [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { ImGuiPass(f, cmd);          });
}

void SceneRenderer::RenderScene(const RenderFrame& frame)
{
    HMN_PROFILE_FUNCTION();
    m_RenderGraph.SetRenderScale(frame.renderScale);
    m_RenderGraph.Execute(frame);
}

void SceneRenderer::GeometryPass(const RenderFrame& frame, CommandList& /*baseCmd*/)
{
    HMN_PROFILE_FUNCTION();
    if (frame.bakeEnvMap && frame.bakedEnvMapOut)
    {
        auto baked       = EnvironmentProbe::Bake(frame.bakeCapPos, frame, frame.bakeResolution);
        auto irradiance  = EnvironmentProbe::ConvolveIrradiance(baked);
        auto prefiltered = EnvironmentProbe::PrefilterSpecular(baked);
        frame.bakedEnvMapOut->map         = std::move(baked);
        frame.bakedEnvMapOut->irradiance  = std::move(irradiance);
        frame.bakedEnvMapOut->prefiltered = std::move(prefiltered);
        frame.bakedEnvMapOut->ready.store(true, std::memory_order_release);
    }

    if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;

    const auto hdr = m_RenderGraph.GetFBO("hdr");
    hdr->Bind();

    const auto& hdrSpec = hdr->GetSpecification();
    RenderCommand::SetViewport(0, 0, hdrSpec.Width, hdrSpec.Height);

    // Re-establish geometry state here: bake block above leaves state as NoDepthNoCull.
    auto cmd = RenderCommand::SetPipelineState(PipelineState::DepthTestWriteCull());
    RenderCommand::SetClearColor(frame.clearColor);
    RenderCommand::Clear();

    // Paint the background before any content. Depth disabled so it fills
    // every pixel; the 3D pass later overwrites wherever geometry is drawn.
    if (m_SkyboxShader && frame.skybox && frame.skybox->GetRendererID() != 0)
    {
        auto skyCmd = RenderCommand::SetPipelineState(PipelineState::NoDepthNoCull());
        skyCmd.BindShader(m_SkyboxShader);
        skyCmd.SetInt   (m_SkyboxShader, "u_Equirect",    0);
        skyCmd.SetMat4  (m_SkyboxShader, "u_InvViewProj", glm::inverse(frame.viewProjection3D));
        skyCmd.SetFloat3(m_SkyboxShader, "u_CamPos",      frame.cameraWorldPos);
        skyCmd.SetFloat (m_SkyboxShader, "u_Intensity",   frame.skyboxIntensity);
        skyCmd.BindTexture(0, frame.skybox->GetRendererID());
        skyCmd.DrawFullscreenTriangle();
        skyCmd.Submit();
        cmd = RenderCommand::SetPipelineState(PipelineState::DepthTestWriteCull());
    }

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
        auto smokeCmd = RenderCommand::SetPipelineState(PipelineState::AlphaBlendDepthTest());
        smokeCmd.BindShader(m_SmokeQuadShader);
        for (const auto& sq : frame.smokeQuads)
        {
            smokeCmd.SetMat4  (m_SmokeQuadShader, "u_Model",       sq.transform);
            smokeCmd.SetFloat3(m_SmokeQuadShader, "u_ColorDark",   sq.colorDark);
            smokeCmd.SetFloat3(m_SmokeQuadShader, "u_ColorLit",    sq.colorLit);
            smokeCmd.SetFloat (m_SmokeQuadShader, "u_Opacity",     sq.opacity);
            smokeCmd.SetFloat (m_SmokeQuadShader, "u_ScrollSpeed", sq.scrollSpeed);
            smokeCmd.SetFloat (m_SmokeQuadShader, "u_Time",        sq.time);
            smokeCmd.SetFloat (m_SmokeQuadShader, "u_Seed",        sq.seed);
            smokeCmd.DrawUnitQuad();
        }
        smokeCmd.Submit();
    }

    if (!frame.fireQuads.empty() && m_FireQuadShader)
    {
        auto fireCmd = RenderCommand::SetPipelineState(PipelineState::AdditiveBlendDepthTest());
        fireCmd.BindShader(m_FireQuadShader);
        for (const auto& fq : frame.fireQuads)
        {
            fireCmd.SetMat4  (m_FireQuadShader, "u_Model",       fq.transform);
            fireCmd.SetFloat3(m_FireQuadShader, "u_ColorCore",   fq.colorCore);
            fireCmd.SetFloat3(m_FireQuadShader, "u_ColorMid",    fq.colorMid);
            fireCmd.SetFloat3(m_FireQuadShader, "u_ColorEdge",   fq.colorEdge);
            fireCmd.SetFloat (m_FireQuadShader, "u_Intensity",   fq.intensity);
            fireCmd.SetFloat (m_FireQuadShader, "u_ScrollSpeed", fq.scrollSpeed);
            fireCmd.SetFloat (m_FireQuadShader, "u_Time",        fq.time);
            fireCmd.SetFloat (m_FireQuadShader, "u_Seed",        fq.seed);
            fireCmd.DrawUnitQuad();
        }
        fireCmd.Submit();
    }

    // 2D overlay - screen-space content on top of all 3D geometry.
    Renderer2D::BeginScene(frame.viewProjection2D);

    for (const auto&[transform, color, uvMin, uvMax, texture] : frame.quads)
        Renderer2D::PushQuad(transform, color, uvMin, uvMax, texture);

    Renderer2D::Flush();

    for (const auto& t : frame.texts)
        Renderer2D::DrawStringMultiline(t.text, t.font, t.transform, t.color, t.colorRight);

    Renderer2D::EndScene();

    hdr->Unbind();
}

void SceneRenderer::ImGuiPass(const RenderFrame& frame, CommandList& /*cmd*/)
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

void SceneRenderer::AutoExposurePass(const RenderFrame& frame, CommandList& /*cmd*/)
{
    HMN_PROFILE_FUNCTION();
    if (!frame.toneMappingEnabled) return;
    auto hdr = m_RenderGraph.GetFBO("hdr");
    const auto& spec = hdr->GetSpecification();
    m_AutoExposure.Compute(hdr->GetColorAttachmentRendererID(),
                           spec.Width, spec.Height);
}

void SceneRenderer::BloomThresholdPass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();
    if (!frame.bloomEnabled) return;
    auto bloom = m_RenderGraph.GetFBO("bloom");
    bloom->Bind();
    auto& spec = bloom->GetSpecification();
    RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
    RenderCommand::SetClearColor({ 0.f, 0.f, 0.f, 1.f });
    RenderCommand::Clear();
    cmd.BindShader(m_ThresholdShader);
    cmd.SetInt  (m_ThresholdShader, "u_HDR",       0);
    cmd.SetFloat(m_ThresholdShader, "u_Threshold", frame.bloomThreshold);
    cmd.BindTexture(0, m_RenderGraph.GetFBO("hdr")->GetColorAttachmentRendererID());
    cmd.DrawFullscreenTriangle();
    cmd.Submit(); // submit while bloom FBO is still bound; RenderGraph Submit() will be a no-op
    bloom->Unbind();
}

void SceneRenderer::BloomBlurHPass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();
    if (!frame.bloomEnabled) return;
    auto dst = m_RenderGraph.GetFBO("bloom_temp");
    dst->Bind();
    auto& spec = dst->GetSpecification();
    RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
    cmd.BindShader(m_BlurShader);
    cmd.SetInt(m_BlurShader, "u_Src",        0);
    cmd.SetInt(m_BlurShader, "u_Horizontal", 1);
    cmd.BindTexture(0, m_RenderGraph.GetFBO("bloom")->GetColorAttachmentRendererID());
    cmd.DrawFullscreenTriangle();
    cmd.Submit(); // submit while bloom_temp FBO is still bound
    dst->Unbind();
}

void SceneRenderer::BloomBlurVPass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();
    if (!frame.bloomEnabled) return;
    const auto dst = m_RenderGraph.GetFBO("bloom");
    dst->Bind();
    auto& spec = dst->GetSpecification();
    RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
    cmd.BindShader(m_BlurShader);
    cmd.SetInt(m_BlurShader, "u_Src",        0);
    cmd.SetInt(m_BlurShader, "u_Horizontal", 0);
    cmd.BindTexture(0, m_RenderGraph.GetFBO("bloom_temp")->GetColorAttachmentRendererID());
    cmd.DrawFullscreenTriangle();
    cmd.Submit(); // submit while bloom FBO is still bound
    dst->Unbind();
}

void SceneRenderer::CompositePass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();

    if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;
    auto hdrFBO = m_RenderGraph.GetFBO("hdr");
    if (!hdrFBO) return;
    RenderCommand::SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
    cmd.BindShader(m_CompositeShader);
    cmd.SetInt  (m_CompositeShader, "u_HDR",               0);
    cmd.SetInt  (m_CompositeShader, "u_Bloom",             1);
    cmd.SetFloat(m_CompositeShader, "u_Exposure",          m_AutoExposure.GetExposure());
    cmd.SetFloat(m_CompositeShader, "u_BloomStrength",     frame.bloomStrength);
    cmd.SetInt  (m_CompositeShader, "u_BloomEnabled",       frame.bloomEnabled       ? 1 : 0);
    cmd.SetInt  (m_CompositeShader, "u_ToneMappingEnabled", frame.toneMappingEnabled ? 1 : 0);
    cmd.BindTexture(0, hdrFBO->GetColorAttachmentRendererID());
    cmd.BindTexture(1, m_RenderGraph.GetFBO("bloom")->GetColorAttachmentRendererID());
    cmd.DrawFullscreenTriangle();
    // RenderGraph calls Submit() after — bloom FBO was unbound by BloomBlurVPass, so draws to window
}

}
