#include "hmnpch.h"
#include "Hominem/Renderer/SceneRenderer.h"
#include "Hominem/Core/Profiler.h"

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>

#include "Hominem/Renderer/RHI/RenderCommand.h"
#include "Hominem/Renderer/Frame/RenderSettings.h"
#include "Hominem/Renderer/2D/Renderer2D.h"
#include "Hominem/Renderer/ForwardPlusRenderer.h"
#include "Hominem/Renderer/Lighting/EnvironmentProbe.h"

namespace Hominem {

namespace {

// Low-discrepancy, so jitter offsets spread evenly over the pixel rather than clumping
// and leaving parts of it unsampled.
float Halton(uint32_t i, uint32_t base)
{
    float f = 1.f, r = 0.f;
    while (i > 0)
    {
        f /= (float)base;
        r += f * (float)(i % base);
        i /= base;
    }
    return r;
}

constexpr uint32_t k_TAASampleCount = 16;

}

void SceneRenderer::SetImGuiCallbacks(std::function<void()> waitFn,
                                      std::function<void()> notifyFn)
{
    m_WaitImGui   = std::move(waitFn);
    m_NotifyImGui = std::move(notifyFn);
}

void SceneRenderer::Init()
{
    m_AutoExposure.Init(ComputeShader::Create("engine://Shaders/luminance.comp"));

    RenderSettings::DetectRecommendedRenderScale();
    ForwardPlusRenderer::InitForwardPlus();
    // Sits with the GPU/GL lines so a bug report carries the GPU and the state together.
    RenderSettings::LogAll();

    SetupPasses();

    auto lib = ShaderLibrary::Engine();
    m_ThresholdShader  = lib->Load("engine://Shaders/bloom_threshold.glsl");
    m_BlurShader       = lib->Load("engine://Shaders/bloom_blur.glsl");
    m_CompositeShader  = lib->Load("engine://Shaders/composite.glsl");
    m_SkyboxShader     = lib->Load("engine://Shaders/skybox.glsl");
    m_FireQuadShader   = lib->Load("engine://Shaders/fire_quad.glsl");
    m_SmokeQuadShader  = lib->Load("engine://Shaders/smoke_quad.glsl");
    m_VkBlitShader     = lib->Load("engine://Shaders/vk_blit.glsl");
    m_TAAResolveShader = lib->Load("engine://Shaders/taa_resolve.glsl");
}

void SceneRenderer::Shutdown()
{
    m_VkBlitShader.reset();
    m_TAAResolveShader.reset();
    m_ThresholdShader.reset();
    m_BlurShader.reset();
    m_CompositeShader.reset();
}

void SceneRenderer::SetupPasses()
{
    // HDR has 2 color attachments: [0] = rendered scene, [1] = RG velocity in UV space
    m_RenderGraph.AddFBO("hdr",        FramebufferFormat::RGBA16F, 1.0f, 2);
    // Two targets: the resolve's output is next frame's input, and a pass cannot sample
    // the texture it renders into. Record() swaps the aliases each frame.
    m_RenderGraph.AddFBO("taa_0",      FramebufferFormat::RGBA16F, 1.0f);
    m_RenderGraph.AddFBO("taa_1",      FramebufferFormat::RGBA16F, 1.0f);
    m_RenderGraph.AddFBO("bloom",      FramebufferFormat::RGBA8,   0.25f);
    m_RenderGraph.AddFBO("bloom_temp", FramebufferFormat::RGBA8,   0.25f);

    m_RenderGraph.SetAlias("taa_curr", "taa_0");
    m_RenderGraph.SetAlias("taa_prev", "taa_1");

    m_RenderGraph.AddPass("scene",
        PipelineState::DepthTestWriteCull(),
        PassBuilder{}.WriteFBO("hdr"),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { GeometryPass(f, cmd); });

    // Everything downstream reads taa_curr instead of hdr, auto-exposure included — it
    // would otherwise meter an unresolved image and flicker against the resolved one.
    m_RenderGraph.AddPass("taa_resolve",
        PipelineState::NoDepthNoCull(),
        PassBuilder{}.Read("hdr.color",      Slot::Color0)
                     .Read("taa_prev.color", Slot::Color1)
                     .Read("hdr.color1",     Slot::Color2)
                     .Read("hdr.depth",      Slot::Depth)
                     .WriteFBO("taa_curr"),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { TAAResolvePass(f, cmd); });

    m_RenderGraph.AddPass("auto_exposure",
        PipelineState::NoDepthNoCull(),
        PassBuilder{},
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { AutoExposurePass(f, cmd); });

    m_RenderGraph.AddPass("bloom_threshold",
        PipelineState::NoDepthNoCull(),
        PassBuilder{}.Read("taa_curr.color", Slot::Color0).WriteFBO("bloom"),
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
        PassBuilder{}.Read("taa_curr.color", Slot::Color0).Read("bloom.color", Slot::Color1),
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

void SceneRenderer::PrepareTemporal(RenderFrame& frame)
{
    frame.viewProjection3DUnjittered = frame.viewProjection3D;
    frame.proj3DUnjittered           = frame.proj3D;
    frame.prevViewProjection3D       = m_PrevViewProjection;
    frame.taaEnabled                 = RenderSettings::TAA;
    frame.taaJitter                  = glm::vec2(0.f);
    frame.taaReset                   = true;

    if (RenderSettings::ConsumeTAAHistoryReset())
        m_TAAResetPending = true;

    if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;

    // Mirrors RenderGraph::OnResize for a scale-1.0 target. Recomputed rather than read
    // off the FBO because the graph resizes on the render thread, a frame behind this.
    const float    scale   = std::clamp(frame.renderScale, 0.25f, 1.0f);
    const uint32_t renderW = std::max(1u, (uint32_t)(frame.viewportWidth  * scale));
    const uint32_t renderH = std::max(1u, (uint32_t)(frame.viewportHeight * scale));

    if (renderW != m_TAALastRenderW || renderH != m_TAALastRenderH)
    {
        m_TAAResetPending = true;  // resize recreates the FBOs, so the history is gone
        m_TAALastRenderW  = renderW;
        m_TAALastRenderH  = renderH;
    }

    frame.taaReset = m_TAAResetPending || !frame.taaEnabled;

    if (frame.taaEnabled)
    {
        const uint32_t n = (m_TAAFrameIndex % k_TAASampleCount) + 1;
        const glm::vec2 jitterPx {
            (Halton(n, 2) - 0.5f) * RenderSettings::TAAJitterScale,
            (Halton(n, 3) - 0.5f) * RenderSettings::TAAJitterScale,
        };
        frame.taaJitter = jitterPx / glm::vec2(renderW, renderH);

        // Sub-pixel shift of the whole projection — moves where inside each pixel the
        // single sample lands. Without it every frame samples the same point and the
        // accumulation adds no coverage.
        frame.proj3D[2][0] += 2.f * jitterPx.x / (float)renderW;
        frame.proj3D[2][1] += 2.f * jitterPx.y / (float)renderH;
        frame.viewProjection3D = frame.proj3D * frame.view3D;

        m_TAAFrameIndex++;
    }

    m_PrevViewProjection = frame.viewProjection3DUnjittered;
    m_TAAResetPending    = false;
}

std::vector<CommandList> SceneRenderer::Record(const RenderFrame& frame)
{
    m_TAAHistoryIdx ^= 1u;
    m_RenderGraph.SetAlias("taa_curr", m_TAAHistoryIdx == 0 ? "taa_0" : "taa_1");
    m_RenderGraph.SetAlias("taa_prev", m_TAAHistoryIdx == 0 ? "taa_1" : "taa_0");

    return m_RenderGraph.Record(frame);
}

void SceneRenderer::TAAResolvePass(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();

    if (frame.viewportWidth == 0 || frame.viewportHeight == 0 || !m_TAAResolveShader) return;

    const auto hdr = m_RenderGraph.GetFBO("hdr");
    if (!hdr) return;
    const auto& spec = hdr->GetSpecification();

    // FBO bound, viewport set, texture slots bound — all from PassBuilder.
    cmd.BindShader(m_TAAResolveShader);
    cmd.SetInt(m_TAAResolveShader, "u_Current",  (int)Slot::Color0);
    cmd.SetInt(m_TAAResolveShader, "u_History",  (int)Slot::Color1);
    cmd.SetInt(m_TAAResolveShader, "u_Velocity", (int)Slot::Color2);
    cmd.SetInt(m_TAAResolveShader, "u_Depth",    (int)Slot::Depth);
    cmd.SetInt(m_TAAResolveShader, "u_UseVelocity", RenderSettings::TAAVelocity ? 1 : 0);

    // Packed into a vec4 — CommandList has no SetFloat2, and one uniform is cheaper than
    // threading another setter through Shader and every backend.
    cmd.SetFloat4(m_TAAResolveShader, "u_TexelSize",
        glm::vec4(1.f / (float)spec.Width, 1.f / (float)spec.Height,
                  (float)spec.Width, (float)spec.Height));

    cmd.SetMat4(m_TAAResolveShader, "u_InvViewProj",
                glm::inverse(frame.viewProjection3DUnjittered));
    cmd.SetMat4(m_TAAResolveShader, "u_PrevViewProj", frame.prevViewProjection3D);

    const float feedbackMax = glm::clamp(RenderSettings::TAAFeedbackMax, 0.f, 0.99f);
    cmd.SetFloat(m_TAAResolveShader, "u_FeedbackMin",
                 glm::clamp(RenderSettings::TAAFeedbackMin, 0.f, feedbackMax));
    cmd.SetFloat(m_TAAResolveShader, "u_FeedbackMax", feedbackMax);
    cmd.SetInt(m_TAAResolveShader, "u_Reset",       frame.taaReset ? 1 : 0);
    cmd.SetInt(m_TAAResolveShader, "u_UseDilation", RenderSettings::TAADilation ? 1 : 0);
    cmd.SetInt(m_TAAResolveShader, "u_DebugView",   RenderSettings::TAADebugView);

    cmd.DrawFullscreenTriangle();
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

    const uint32_t barHeight = (uint32_t)(hdrSpec.Height * glm::clamp(frame.bottomBarFraction, 0.f, 1.f));
    if (barHeight > 0)
        cmd.SetViewport(0, barHeight, hdrSpec.Width, hdrSpec.Height - barHeight);

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
    ForwardPlusRenderer::SceneData scene = ForwardPlusRenderer::BeginScene(frame, cmd, hdrSpec.Width, hdrSpec.Height);

    scene.DDGI = frame.vulkanDDGI;
    if (const auto shared = m_SharedImages.load(std::memory_order_acquire))
    {
        scene.DDGIIrradianceID = shared->Get(SharedImageName::DDGIIrradiance);
        scene.DDGIDistanceID   = shared->Get(SharedImageName::DDGIDistance);
    }

    for (const auto& sm : frame.staticMeshes)
        ForwardPlusRenderer::DrawStaticMesh(*sm.mesh, sm.transform, cmd, scene, &sm.prevTransform);

    for (const auto& m : frame.meshes)
    {
        if (m.overrideShader) ForwardPlusRenderer::SetOverrideShader(m.overrideShader);
        m.mesh->DispatchSkinning(m.bones, cmd);
        ForwardPlusRenderer::DrawSkinnedMesh(*m.mesh, m.transform, cmd, scene, &m.prevTransform);
        if (m.overrideShader) ForwardPlusRenderer::ClearOverrideShader();
    }

    cmd.Invoke([]() { ForwardPlusRenderer::EndScene(); });

    // Everything from here on — debug gizmos, smoke, fire, all 2D — is blended and has no
    // velocity output. GL blend state applies to every draw buffer, so leaving writes on
    // would fade the values the opaque pass wrote. Masked instead: those pixels reproject
    // as whatever surface is behind them, a better guess for thin effects and screen-space
    // UI than a blended-toward-zero vector. Restored at the end of the pass.
    cmd.SetColorMask(1, false);

    if (frame.debugLights && !frame.lights.empty())
        cmd.Invoke([lights = frame.lights]() { ForwardPlusRenderer::DrawDebugLights(lights); });

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

    // 2D content is screen-space static, so zero velocity is correct for it — but it is
    // alpha-blended too, so it stays masked and keeps whatever the scene wrote beneath.
    if (barHeight > 0)
        cmd.SetViewport(0, 0, hdrSpec.Width, hdrSpec.Height);

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

    cmd.SetColorMask(1, true); // GL global state — must not leak into next frame's opaque pass
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
        auto resolved = m_RenderGraph.GetFBO("taa_curr");
        const auto& spec = resolved->GetSpecification();
        uint32_t colorAttachment = resolved->GetColorAttachmentRendererID();
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
    // Trace-only draws produce no pixels; skipping the blit then also skips a full-screen
    // pass every frame. Anything Vulkan does draw composites over the GL scene by alpha.
    const bool vulkanDrewSomething = !frame.vulkanPasses.empty()
                                  || HasRasterDraws(frame.vulkanMeshDraws)
                                  || !frame.vulkanDebugSpheres.empty()
                                  || frame.vulkanDDGI.showSurfels;
    if (!m_SharedVkTexture || !vulkanDrewSomething) return;

    cmd.BindShader (m_VkBlitShader);
    cmd.SetInt     (m_VkBlitShader, "u_VkTexture", 0);
    cmd.BindTexture(0, m_SharedVkTexture);
    cmd.DrawFullscreenTriangle();
}

}
