#include "hmnpch.h"
#include "EnvironmentProbe.h"
#include "Renderer3D.h"
#include "Hominem/Renderer/RenderThread.h"
#include "Hominem/Renderer/RenderCommand.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Hominem {

// Standard OpenGL cubemap face directions
static const glm::vec3 s_FaceTargets[6] = {
    { 1, 0, 0}, {-1, 0, 0},   // +X, -X
    { 0, 1, 0}, { 0,-1, 0},   // +Y, -Y
    { 0, 0, 1}, { 0, 0,-1},   // +Z, -Z
};
static const glm::vec3 s_FaceUps[6] = {
    { 0,-1, 0}, { 0,-1, 0},   // +X, -X: up is -Y
    { 0, 0, 1}, { 0, 0,-1},   // +Y, -Y: up is ±Z
    { 0,-1, 0}, { 0,-1, 0},   // +Z, -Z: up is -Y
};

Ref<TextureCube> EnvironmentProbe::Bake(const glm::vec3& capturePos,
                                         const RenderFrame& sceneFrame,
                                         uint32_t resolution)
{
    RenderThread::AssertRenderThread();

    auto cube = TextureCube::CreateEmpty(resolution);
    cube->EnsureCreated();

    // Temp FBO + depth renderbuffer
    uint32_t fbo      = RenderCommand::GenFramebuffer();
    uint32_t depthRBO = RenderCommand::GenRenderbuffer();
    RenderCommand::BindRenderbuffer(depthRBO);
    RenderCommand::RenderbufferDepth(resolution, resolution);
    RenderCommand::BindFramebuffer(fbo);
    RenderCommand::AttachRenderbuffer(depthRBO);

    const glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 1000.f);

    RenderCommand::SetViewport(0, 0, resolution, resolution);
    RenderCommand::SetPipelineState(PipelineState::DepthTestWriteCull());
    RenderCommand::SetClearColor({ 0.05f, 0.05f, 0.05f, 1.f });

    for (int face = 0; face < 6; face++)
    {
        RenderCommand::AttachCubeFace(cube->GetRendererID(), face, 0);
        RenderCommand::Clear();

        const glm::mat4 view = glm::lookAt(capturePos,
                                            capturePos + s_FaceTargets[face],
                                            s_FaceUps[face]);

        // Build a minimal frame for this face — env map disabled to avoid recursion
        RenderFrame faceFrame;
        faceFrame.viewProjection3D = proj * view;
        faceFrame.view3D           = view;
        faceFrame.proj3D           = proj;
        faceFrame.cameraWorldPos   = capturePos;
        faceFrame.light            = sceneFrame.light;
        faceFrame.lights           = sceneFrame.lights;
        faceFrame.viewportWidth    = resolution;
        faceFrame.viewportHeight   = resolution;
        faceFrame.envMapIntensity  = 0.f;    // no recursion
        faceFrame.staticMeshes     = sceneFrame.staticMeshes;

        auto cmd = RenderCommand::SetPipelineState(PipelineState::DepthTestWriteCull());
        auto scene = Renderer3D::BeginScene(faceFrame, cmd);
        for (const auto& sm : faceFrame.staticMeshes)
            Renderer3D::DrawStaticMesh(*sm.mesh, sm.transform, cmd, scene);
        cmd.Submit();
        Renderer3D::EndScene();
    }

    cube->GenerateMipmaps();

    RenderCommand::UnbindFramebuffer();
    RenderCommand::DeleteRenderbuffer(depthRBO);
    RenderCommand::DeleteFramebuffer(fbo);

    HMN_CORE_INFO("EnvironmentProbe: baked {}x{} cubemap from ({:.1f},{:.1f},{:.1f})",
                  resolution, resolution, capturePos.x, capturePos.y, capturePos.z);

    return cube;
}

Ref<TextureCube> EnvironmentProbe::ConvolveIrradiance(const Ref<TextureCube>& source, uint32_t resolution)
{
    RenderThread::AssertRenderThread();

    auto cube = TextureCube::CreateEmpty(resolution);
    cube->EnsureCreated();

    auto shader = Renderer3D::GetShaderLibrary()->Get("irradiance_convolve");
    HMN_CORE_ASSERT(shader, "EnvironmentProbe: irradiance_convolve shader not loaded");

    uint32_t fbo = RenderCommand::GenFramebuffer();
    RenderCommand::BindFramebuffer(fbo);

    const glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 10.f);

    auto cmd = RenderCommand::SetPipelineState(PipelineState::NoDepthNoCull());
    cmd.BindShader(shader);
    cmd.SetInt(shader, "u_EnvMap", 0);
    cmd.BindTexture(0, source->GetRendererID());

    for (int face = 0; face < 6; face++)
    {
        RenderCommand::AttachCubeFace(cube->GetRendererID(), face, 0);
        RenderCommand::SetViewport(0, 0, resolution, resolution);

        const glm::mat4 view  = glm::lookAt(glm::vec3(0.f), s_FaceTargets[face], s_FaceUps[face]);
        const glm::mat4 invVP = glm::inverse(proj * view);
        cmd.SetMat4(shader, "u_InvViewProj", invVP);

        cmd.DrawFullscreenTriangle();
        cmd.Submit(); // FBO attachment and viewport change per face
    }

    RenderCommand::UnbindFramebuffer();
    RenderCommand::DeleteFramebuffer(fbo);

    HMN_CORE_INFO("EnvironmentProbe: convolved {}x{} irradiance cubemap", resolution, resolution);

    return cube;
}

Ref<TextureCube> EnvironmentProbe::PrefilterSpecular(const Ref<TextureCube>& source, uint32_t resolution)
{
    RenderThread::AssertRenderThread();

    auto cube = TextureCube::CreateEmpty(resolution);
    cube->EnsureCreated();

    const auto shader = Renderer3D::GetShaderLibrary()->Get("prefilter_convolve");
    HMN_CORE_ASSERT(shader, "EnvironmentProbe: prefilter_convolve shader not loaded");

    const uint32_t fbo = RenderCommand::GenFramebuffer();
    RenderCommand::BindFramebuffer(fbo);

    const glm::mat4 proj      = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 10.f);
    const uint32_t  mipLevels = cube->GetMipLevels();

    auto cmd = RenderCommand::SetPipelineState(PipelineState::NoDepthNoCull());
    cmd.BindShader(shader);
    cmd.SetInt(shader, "u_EnvMap", 0);
    cmd.BindTexture(0, source->GetRendererID());

    for (uint32_t mip = 0; mip < mipLevels; mip++)
    {
        const uint32_t mipRes    = std::max(1u, resolution >> mip);
        const float    roughness = (mipLevels > 1) ? (float)mip / (float)(mipLevels - 1) : 0.f;
        cmd.SetFloat(shader, "u_Roughness", roughness);

        for (int face = 0; face < 6; face++)
        {
            RenderCommand::AttachCubeFace(cube->GetRendererID(), face, mip);
            RenderCommand::SetViewport(0, 0, mipRes, mipRes);

            const glm::mat4 view  = glm::lookAt(glm::vec3(0.f), s_FaceTargets[face], s_FaceUps[face]);
            const glm::mat4 invVP = glm::inverse(proj * view);
            cmd.SetMat4(shader, "u_InvViewProj", invVP);

            cmd.DrawFullscreenTriangle();
            cmd.Submit(); // FBO attachment and viewport change per iteration
        }
    }

    RenderCommand::UnbindFramebuffer();
    RenderCommand::DeleteFramebuffer(fbo);

    HMN_CORE_INFO("EnvironmentProbe: prefiltered {}x{} specular cubemap, {} mips",
                  resolution, resolution, mipLevels);

    return cube;
}

Ref<Texture2D> EnvironmentProbe::BakeBRDFLUT(uint32_t resolution)
{
    RenderThread::AssertRenderThread();

    auto lut = Texture2D::Create(resolution, resolution, TextureFormat::RG16F);
    lut->EnsureCreated();

    auto shader = Renderer3D::GetShaderLibrary()->Get("brdf_lut");
    HMN_CORE_ASSERT(shader, "EnvironmentProbe: brdf_lut shader not loaded");

    const uint32_t fbo = RenderCommand::GenFramebuffer();
    RenderCommand::BindFramebuffer(fbo);
    RenderCommand::Attach2DTexture(lut->GetRendererID());
    RenderCommand::SetViewport(0, 0, resolution, resolution);

    auto cmd = RenderCommand::SetPipelineState(PipelineState::NoDepthNoCull());
    cmd.BindShader(shader);
    cmd.DrawFullscreenTriangle();
    cmd.Submit();

    RenderCommand::UnbindFramebuffer();
    RenderCommand::DeleteFramebuffer(fbo);

    HMN_CORE_INFO("EnvironmentProbe: baked {}x{} BRDF LUT", resolution, resolution);

    return lut;
}

}
