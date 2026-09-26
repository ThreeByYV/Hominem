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
#include "Hominem/Renderer/PostProcess/Upscaling/TemporalJitter.h"
#include "Hominem/Renderer/PostProcess/Upscaling/UpscalerFactory.h"

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

    RenderSettings::DetectRecommendedRenderScale();
    ForwardPlusRenderer::InitForwardPlus();
    // Sits with the GPU/GL lines so a bug report carries the GPU and the state together.
    RenderSettings::LogAll();

    m_Upscaler = CreateUpscaler(UpscalerBackend::TAA);
    m_Upscaler->Init();
    HMN_CORE_INFO("Upscaler: {0}", m_Upscaler->GetName());

    SetupPasses();

    auto lib = ShaderLibrary::Engine();
    m_ThresholdShader  = lib->Load("engine://Shaders/bloom_threshold.glsl");
    m_BlurShader       = lib->Load("engine://Shaders/bloom_blur.glsl");
    m_CompositeShader  = lib->Load("engine://Shaders/composite.glsl");
    m_SkyboxShader     = lib->Load("engine://Shaders/skybox.glsl");
    m_FireQuadShader   = lib->Load("engine://Shaders/fire_quad.glsl");
    m_SmokeQuadShader  = lib->Load("engine://Shaders/smoke_quad.glsl");
    m_VkBlitShader     = lib->Load("engine://Shaders/vk_blit.glsl");
}

void SceneRenderer::Shutdown()
{
    m_VkBlitShader.reset();
    m_Upscaler.reset();
    m_ThresholdShader.reset();
    m_BlurShader.reset();
    m_CompositeShader.reset();
}

void SceneRenderer::SetupPasses()
{
    // HDR has 2 color attachments: [0] = rendered scene, [1] = RG velocity in UV space
    m_RenderGraph.AddFBO("hdr",        FramebufferFormat::RGBA16F, 1.0f, 2);
    // Post-processing runs after the upscale, so bloom follows the output size.
    constexpr auto Output = RenderGraph::Resolution::Output;
    m_RenderGraph.AddFBO("bloom",      FramebufferFormat::RGBA8,   0.25f, 1, Output);
    m_RenderGraph.AddFBO("bloom_temp", FramebufferFormat::RGBA8,   0.25f, 1, Output);

    m_Upscaler->DeclareResources(m_RenderGraph);

    m_RenderGraph.AddPass("scene",
        PipelineState::DepthTestWriteCull(),
        PassBuilder{}.WriteFBO("hdr"),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { GeometryPass(f, cmd); });

    // Everything downstream reads the upscaled output, auto-exposure included, so it
    // meters the resolved image.
    m_RenderGraph.AddPass("upscale",
        PipelineState::NoDepthNoCull(),
        PassBuilder{}.Read("hdr.color",  Slot::Color0)
                     .Read("hdr.color1", Slot::Color2)
                     .Read("hdr.depth",  Slot::Depth)
                     .WriteFBO(Upscaler::OutputTarget),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { UpscalePass(f, cmd); });

    m_RenderGraph.AddPass("auto_exposure",
        PipelineState::NoDepthNoCull(),
        PassBuilder{},
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { AutoExposurePass(f, cmd); });

    m_RenderGraph.AddPass("bloom_threshold",
        PipelineState::NoDepthNoCull(),
        PassBuilder{}.Read("upscaled.color", Slot::Color0).WriteFBO("bloom"),
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
        PassBuilder{}.Read("upscaled.color", Slot::Color0).Read("bloom.color", Slot::Color1),
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { CompositePass(f, cmd); });

    m_RenderGraph.AddPass("vk_output",
        PipelineState::AlphaBlendNoDepth(),
        PassBuilder{},
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { VulkanBlitPass(f, cmd); });

    // Screen-space 2D (perspective scenes) at output resolution, after the upscale.
    m_RenderGraph.AddPass("overlay_2d",
        PipelineState::AlphaBlendNoDepth(),
        PassBuilder{},
        [this](RenderGraph&, const RenderFrame& f, CommandList& cmd) { Overlay2DPass(f, cmd); });

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
    const UpscalerCaps caps          = m_Upscaler->GetCaps();
    frame.taaEnabled                 = RenderSettings::TAA && caps.temporal;
    frame.taaJitter                  = glm::vec2(0.f);
    frame.textureLodBias             = 0.f;
    frame.taaReset                   = true;

    if (RenderSettings::ConsumeTAAHistoryReset())
        m_TAAResetPending = true;

    if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;

    // Mirrors RenderGraph::OnResize for a render-resolution target. Recomputed rather than read
    // off the FBO because the graph resizes on the render thread, a frame behind this.
    frame.renderScale = m_Upscaler->GetRenderScale(frame.renderScale, { frame.viewportWidth, frame.viewportHeight });
    const uint32_t renderW = RenderGraph::ScaledSize(frame.viewportWidth,  frame.renderScale);
    const uint32_t renderH = RenderGraph::ScaledSize(frame.viewportHeight, frame.renderScale);

    if (renderW != m_TAALastRenderW || renderH != m_TAALastRenderH)
    {
        m_TAAResetPending = true;  // resize recreates the FBOs, so the history is gone
        m_TAALastRenderW  = renderW;
        m_TAALastRenderH  = renderH;
    }

    frame.taaReset = m_TAAResetPending || !frame.taaEnabled;

    if (frame.taaEnabled)
    {
        // Textures are sampled for the output resolution, not the render one — the jittered
        // frames add back the detail a lower mip would have thrown away. Without temporal
        // accumulation there is nothing to add it back, so the bias stays at zero.
        frame.textureLodBias = TemporalJitter::MipBias(renderW, frame.viewportWidth, caps.mipBiasOffset);

        const uint32_t  phases   = TemporalJitter::PhaseCount(renderW, frame.viewportWidth);
        const glm::vec2 jitterPx = TemporalJitter::Offset(m_TAAFrameIndex, phases)
                                 * RenderSettings::TAAJitterScale;
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
    m_Upscaler->BeginFrame(m_RenderGraph);

    return m_RenderGraph.Record(frame);
}

void SceneRenderer::UpscalePass(const RenderFrame& frame, CommandList& cmd)
{
    if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;

    const auto hdr = m_RenderGraph.GetFBO("hdr");
    if (!hdr) return;
    const auto& spec = hdr->GetSpecification();

    // The FBO's actual size; the graph resizes a frame behind PrepareTemporal.
    UpscalerInputs in;
    in.renderSize         = { spec.Width, spec.Height };
    in.jitterUV           = frame.taaJitter;
    in.viewProjUnjittered = frame.viewProjection3DUnjittered;
    in.prevViewProj       = frame.prevViewProjection3D;
    in.reset              = frame.taaReset;

    m_Upscaler->Evaluate(m_RenderGraph, in, cmd);
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

    // World-space 2D (orthographic scenes) is part of the scene; a HUD is drawn later in
    // Overlay2DPass. Zero velocity is right for it — it's alpha-blended, so it stays masked
    // and keeps whatever the scene wrote beneath.
    if (!frame.overlay2D)
    {
        if (barHeight > 0)
            cmd.SetViewport(0, 0, hdrSpec.Width, hdrSpec.Height);
        Draw2D(frame, cmd);
    }

    cmd.SetColorMask(1, true); // GL global state — must not leak into next frame's opaque pass
    // FBO unbound by graph after this fn returns.
}

void SceneRenderer::Overlay2DPass(const RenderFrame& frame, CommandList& cmd)
{
    if (!frame.overlay2D || frame.viewportWidth == 0 || frame.viewportHeight == 0) return;
    cmd.SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
    Draw2D(frame, cmd);
}

void SceneRenderer::Draw2D(const RenderFrame& frame, CommandList& cmd)
{
    // Captured by value: `frame` is only valid during recording, not at Submit() time.
    cmd.Invoke([vp = frame.viewProjection2D, quads = frame.quads, texts = frame.texts]()
    {
        Renderer2D::BeginScene(vp);

        for (const auto&[transform, color, uvMin, uvMax, texture] : quads)
            Renderer2D::PushQuad(transform, color, uvMin, uvMax, texture);

        Renderer2D::Flush();

        for (const auto& t : texts)
            Renderer2D::DrawStringMultiline(t.text, t.font, t.transform, t.color, t.colorRight);

        Renderer2D::EndScene();
    });
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
        auto resolved = m_RenderGraph.GetFBO(Upscaler::OutputTarget);
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
