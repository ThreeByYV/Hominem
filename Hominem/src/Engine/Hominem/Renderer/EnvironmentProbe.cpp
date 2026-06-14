#include "hmnpch.h"
#include "EnvironmentProbe.h"
#include "Renderer3D.h"
#include "Hominem/Renderer/RenderThread.h"
#include "Hominem/Renderer/RenderCommand.h"

#include <glad/glad.h>
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
    uint32_t fbo, depthRBO;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);

    const glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 1000.f);

    for (int face = 0; face < 6; face++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                               cube->GetRendererID(), 0);

        glViewport(0, 0, (GLsizei)resolution, (GLsizei)resolution);
        glClearColor(0.05f, 0.05f, 0.05f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

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
        faceFrame.lights           = sceneFrame.lights;  // needed for accurate bake lighting
        faceFrame.viewportWidth    = resolution;
        faceFrame.viewportHeight   = resolution;
        faceFrame.envMapIntensity  = 0.f;    // no recursion
        faceFrame.staticMeshes     = sceneFrame.staticMeshes;

        Renderer3D::BeginScene(faceFrame);
        for (const auto& sm : faceFrame.staticMeshes)
            Renderer3D::DrawStaticMesh(*sm.mesh, sm.transform);
        Renderer3D::EndScene();
    }

    // Mipmaps for trilinear filtering when sampled at grazing angles
    cube->GenerateMipmaps();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &depthRBO);
    glDeleteFramebuffers(1, &fbo);

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

    // Color-only FBO, no depth needed for a fullscreen-triangle convolution.
    uint32_t fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    const glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 10.f);

    RenderCommand::SetDepthTestEnabled(false);
    shader->Bind();
    shader->SetInt("u_EnvMap", 0);
    RenderCommand::BindTexture(0, source->GetRendererID());

    for (int face = 0; face < 6; face++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                               cube->GetRendererID(), 0);

        glViewport(0, 0, (GLsizei)resolution, (GLsizei)resolution);

        const glm::mat4 view   = glm::lookAt(glm::vec3(0.f), s_FaceTargets[face], s_FaceUps[face]);
        const glm::mat4 invVP  = glm::inverse(proj * view);
        shader->SetMat4("u_InvViewProj", invVP);

        RenderCommand::DrawFullscreenTriangle();
    }

    RenderCommand::SetDepthTestEnabled(true);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    HMN_CORE_INFO("EnvironmentProbe: convolved {}x{} irradiance cubemap", resolution, resolution);

    return cube;
}

}
