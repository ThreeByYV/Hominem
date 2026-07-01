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
    m_VkBlitShader    = Renderer3D::GetShaderLibrary()->Get("vk_blit");
}

void SceneRenderer::Shutdown()
{
    m_VkBlitShader.reset();
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

    m_RenderGraph.AddPass("scene",
        PipelineState::DepthTestWriteCull(),
        PassBuilder{}.WriteFBO("hdr"),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { GeometryPass(f, cmd); });

    m_RenderGraph.AddPass("auto_exposure",
        PipelineState::NoDepthNoCull(),
        PassBuilder{},
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { AutoExposurePass(f, cmd); });

    m_RenderGraph.AddPass("bloom_threshold",
        PipelineState::NoDepthNoCull(),
        PassBuilder{}.Read("hdr.color", Slot::Color0).WriteFBO("bloom"),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { BloomThresholdPass(f, cmd); });

    m_RenderGraph.AddPass("bloom_blur_h",
        PipelineState::NoDepthNoCull(),
        PassBuilder{}.Read("bloom.color", Slot::Color0).WriteFBO("bloom_temp"),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { BloomBlurHPass(f, cmd); });

    m_RenderGraph.AddPass("bloom_blur_v",
        PipelineState::NoDepthNoCull(),
        PassBuilder{}.Read("bloom_temp.color", Slot::Color0).WriteFBO("bloom"),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { BloomBlurVPass(f, cmd); });

    m_RenderGraph.AddPass("composite",
        PipelineState::NoDepthNoCull(),
        PassBuilder{}.Read("hdr.color", Slot::Color0).Read("bloom.color", Slot::Color1),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { CompositePass(f, cmd); });

    m_RenderGraph.AddPass("vk_output",
        PipelineState::AlphaBlendNoDepth(),
        PassBuilder{},
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { VulkanBlitPass(f, cmd); });

    m_RenderGraph.AddPass("imgui",
        PipelineState::AlphaBlendNoDepth(),
        PassBuilder{},
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { ImGuiPass(f, cmd); });
}

void SceneRenderer::GeometryPass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();

    // One-shot env-map bake — heavy immediate GL work, deferred to the render thread via
    // Invoke (recording happens on the main thread, which has no GL context). Capture `frame`
    // by value: the original is only valid during recording, not at Submit() time.
    if (frame.bakeEnvMap && frame.bakedEnvMapOut)
    {
        cmd.Invoke([frame]() mutable
        {
            auto baked       = EnvironmentProbe::Bake(frame.bakeCapPos, frame, frame.bakeResolution);
            auto irradiance  = EnvironmentProbe::ConvolveIrradiance(baked);
            auto prefiltered = EnvironmentProbe::PrefilterSpecular(baked);
            frame.bakedEnvMapOut->map         = std::move(baked);
            frame.bakedEnvMapOut->irradiance  = std::move(irradiance);
            frame.bakedEnvMapOut->prefiltered = std::move(prefiltered);
            frame.bakedEnvMapOut->ready.store(true, std::memory_order_release);
        });
        // Bake uses immediate RenderCommand GL calls that trash the bound FBO and viewport.
        // Re-establish both so subsequent recorded commands target the right surface at
        // the right size (bake's RenderCommand::SetViewport calls are immediate and override
        // the graph's recorded SetViewport that ran before this Invoke).
        auto hdrAfterBake = m_RenderGraph.GetFBO("hdr");
        if (hdrAfterBake)
        {
            const auto& s = hdrAfterBake->GetSpecification();
            cmd.BindFramebuffer(hdrAfterBake->GetRendererID());
            cmd.SetViewport(0, 0, s.Width, s.Height);
        }
    }

    if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;

    const auto hdr = m_RenderGraph.GetFBO("hdr");
    const auto& hdrSpec = hdr->GetSpecification();
    cmd.SetClearColor(frame.clearColor);
    cmd.Clear();

    // Paint the background before any content. Depth disabled so it fills
    // every pixel; the 3D pass later overwrites wherever geometry is drawn.
    if (m_SkyboxShader && frame.skybox && frame.skybox->GetRendererID() != 0)
    {
        cmd.SetPipelineState(PipelineState::NoDepthNoCull());
        cmd.BindShader(m_SkyboxShader);
        cmd.SetInt   (m_SkyboxShader, "u_Equirect",    0);
        cmd.SetMat4  (m_SkyboxShader, "u_InvViewProj", glm::inverse(frame.viewProjection3D));
        cmd.SetFloat3(m_SkyboxShader, "u_CamPos",      frame.cameraWorldPos);
        cmd.SetFloat (m_SkyboxShader, "u_Intensity",   frame.skyboxIntensity);
        cmd.BindTexture(0, frame.skybox->GetRendererID());
        cmd.DrawFullscreenTriangle();
    }

    // Pass the FBO render dimensions so u_ScreenWidth and tile-culling dispatch
    // match gl_FragCoord — they diverge from frame.viewportWidth when renderScale != 1.
    Renderer3D::SceneData scene = Renderer3D::BeginScene(frame, cmd, hdrSpec.Width, hdrSpec.Height);

    for (const auto& sm : frame.staticMeshes)
        Renderer3D::DrawStaticMesh(*sm.mesh, sm.transform, cmd, scene);

    for (const auto& m : frame.meshes)
    {
        if (m.overrideShader) Renderer3D::SetOverrideShader(m.overrideShader);
        m.mesh->DispatchSkinning(m.bones, cmd);
        Renderer3D::DrawSkinnedMesh(*m.mesh, m.transform, cmd, scene);
        if (m.overrideShader) Renderer3D::ClearOverrideShader();
    }

    cmd.Invoke([]() { Renderer3D::EndScene(); });

    if (frame.debugLights && !frame.lights.empty())
        cmd.Invoke([lights = frame.lights]() { Renderer3D::DrawDebugLights(lights); });

    // Procedural smoke quads — alpha-blended, drawn before the fire quads so the
    // fire glow shows through the smoke. Depth-tested but doesn't write depth.
    if (!frame.smokeQuads.empty() && m_SmokeQuadShader)
    {
        cmd.SetPipelineState(PipelineState::AlphaBlendDepthTest());
        cmd.BindShader(m_SmokeQuadShader);
        for (const auto& sq : frame.smokeQuads)
        {
            cmd.SetMat4  (m_SmokeQuadShader, "u_Model",       sq.transform);
            cmd.SetFloat3(m_SmokeQuadShader, "u_ColorDark",   sq.colorDark);
            cmd.SetFloat3(m_SmokeQuadShader, "u_ColorLit",    sq.colorLit);
            cmd.SetFloat (m_SmokeQuadShader, "u_Opacity",     sq.opacity);
            cmd.SetFloat (m_SmokeQuadShader, "u_ScrollSpeed", sq.scrollSpeed);
            cmd.SetFloat (m_SmokeQuadShader, "u_Time",        sq.time);
            cmd.SetFloat (m_SmokeQuadShader, "u_Seed",        sq.seed);
            cmd.DrawUnitQuad();
        }
    }

    if (!frame.fireQuads.empty() && m_FireQuadShader)
    {
        cmd.SetPipelineState(PipelineState::AdditiveBlendDepthTest());
        cmd.BindShader(m_FireQuadShader);
        for (const auto& fq : frame.fireQuads)
        {
            cmd.SetMat4  (m_FireQuadShader, "u_Model",       fq.transform);
            cmd.SetFloat3(m_FireQuadShader, "u_ColorCore",   fq.colorCore);
            cmd.SetFloat3(m_FireQuadShader, "u_ColorMid",    fq.colorMid);
            cmd.SetFloat3(m_FireQuadShader, "u_ColorEdge",   fq.colorEdge);
            cmd.SetFloat (m_FireQuadShader, "u_Intensity",   fq.intensity);
            cmd.SetFloat (m_FireQuadShader, "u_ScrollSpeed", fq.scrollSpeed);
            cmd.SetFloat (m_FireQuadShader, "u_Time",        fq.time);
            cmd.SetFloat (m_FireQuadShader, "u_Seed",        fq.seed);
            cmd.DrawUnitQuad();
        }
    }

    // Captured by value: `frame` is only valid during recording, not at Submit() time.
    cmd.Invoke([this, vp = frame.viewProjection2D, quads = frame.quads, texts = frame.texts]()
    {
        Renderer2D::BeginScene(vp);

        for (const auto&[transform, color, uvMin, uvMax, texture] : quads)
            Renderer2D::PushQuad(transform, color, uvMin, uvMax, texture);

        Renderer2D::Flush();

        for (const auto& t : texts)
            Renderer2D::DrawStringMultiline(t.text, t.font, t.transform, t.color, t.colorRight);

        Renderer2D::EndScene();
    });
    // FBO unbound by graph after this fn returns.
}

void SceneRenderer::ImGuiPass(const RenderFrame& frame, CommandList& cmd)
{
    if (m_WaitImGui) m_WaitImGui();

    // Render directly into the back buffer (no FBO bound) at full window resolution.
    // This must run after composite so the viewport is correct and UI is unaffected
    // by render scale.
    cmd.SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
    cmd.Invoke([this]()
    {
        ImGui_ImplOpenGL3_NewFrame();
        if (ImDrawData* drawData = ImGui::GetDrawData())
            ImGui_ImplOpenGL3_RenderDrawData(drawData);

        if (m_NotifyImGui) m_NotifyImGui();
    });
}

void SceneRenderer::AutoExposurePass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();
    if (frame.toneMappingEnabled)
    {
        auto hdr = m_RenderGraph.GetFBO("hdr");
        const auto& spec = hdr->GetSpecification();
        uint32_t colorAttachment = hdr->GetColorAttachmentRendererID();
        cmd.Invoke([this, colorAttachment, width = spec.Width, height = spec.Height]()
        {
            m_AutoExposure.Compute(colorAttachment, width, height);
        });
    }
    m_RenderGraph.SetBlackboard(ExposureOutput{ m_AutoExposure.GetExposure() });
}

void SceneRenderer::BloomThresholdPass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();
    if (!frame.bloomEnabled) return;
    // FBO bound, viewport set, slot 0 = hdr.color — all from PassBuilder.
    cmd.SetClearColor({ 0.f, 0.f, 0.f, 1.f });
    cmd.Clear();
    cmd.BindShader(m_ThresholdShader);
    cmd.SetInt  (m_ThresholdShader, "u_HDR",       0);
    cmd.SetFloat(m_ThresholdShader, "u_Threshold", frame.bloomThreshold);
    cmd.DrawFullscreenTriangle();
}

void SceneRenderer::BloomBlurHPass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();
    if (!frame.bloomEnabled) return;
    // FBO bound, viewport set, slot 0 = bloom.color — all from PassBuilder.
    cmd.BindShader(m_BlurShader);
    cmd.SetInt(m_BlurShader, "u_Src",        0);
    cmd.SetInt(m_BlurShader, "u_Horizontal", 1);
    cmd.DrawFullscreenTriangle();
}

void SceneRenderer::BloomBlurVPass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();
    if (!frame.bloomEnabled) return;
    // FBO bound, viewport set, slot 0 = bloom_temp.color — all from PassBuilder.
    cmd.BindShader(m_BlurShader);
    cmd.SetInt(m_BlurShader, "u_Src",        0);
    cmd.SetInt(m_BlurShader, "u_Horizontal", 0);
    cmd.DrawFullscreenTriangle();
}

void SceneRenderer::CompositePass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();

    if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;
    // Slot 0 = hdr.color, slot 1 = bloom.color — bound by graph (PassBuilder).
    // No WriteFBO — renders to the default backbuffer at full window resolution.
    cmd.SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
    cmd.BindShader(m_CompositeShader);
    cmd.SetInt  (m_CompositeShader, "u_HDR",               0);
    cmd.SetInt  (m_CompositeShader, "u_Bloom",             1);
    cmd.SetFloat(m_CompositeShader, "u_Exposure",          m_RenderGraph.GetBlackboard<ExposureOutput>().value);
    cmd.SetFloat(m_CompositeShader, "u_BloomStrength",     frame.bloomStrength);
    cmd.SetInt  (m_CompositeShader, "u_BloomEnabled",       frame.bloomEnabled       ? 1 : 0);
    cmd.SetInt  (m_CompositeShader, "u_ToneMappingEnabled", frame.toneMappingEnabled ? 1 : 0);
    cmd.DrawFullscreenTriangle();
}

void SceneRenderer::VulkanBlitPass(const RenderFrame& frame, CommandList& cmd)
{
    if (!m_SharedVkTexture || frame.vulkanPasses.empty()) return;

    cmd.BindShader (m_VkBlitShader);
    cmd.SetInt     (m_VkBlitShader, "u_VkTexture", 0);
    cmd.BindTexture(0, m_SharedVkTexture);
    cmd.DrawFullscreenTriangle();
}

}
