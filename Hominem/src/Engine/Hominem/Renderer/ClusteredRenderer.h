#pragma once

#include "Hominem/Renderer/RHI/Shader.h"
#include "Hominem/Renderer/RHI/Texture.h"
#include "Hominem/Renderer/RHI/ShaderPermutation.h"
#include "Hominem/Renderer/RHI/Buffer.h"
#include "Hominem/Renderer/RHI/VertexArray.h"
#include "Hominem/Renderer/RHI/StorageBuffer.h"
#include "Hominem/Renderer/Geometry/SkinnedMesh.h"
#include "Hominem/Renderer/Geometry/StaticMesh.h"
#include "Hominem/Renderer/Camera/Frustum.h"
#include "Hominem/Renderer/Frame/RenderFrame.h"

#include <glm/glm.hpp>

namespace Hominem {

// Must match layout(std140, binding=0) uniform SceneUBO in includes/scene_ubo.glsl
struct alignas(16) SceneUBOData
{
    glm::mat4 ViewProjection;    // offset   0, 64 bytes
    glm::vec4 CameraWorldPos;    // offset  64, 16 bytes (xyz used, w padding)
    glm::vec4 LightDirection;    // offset  80
    glm::vec4 LightColor;        // offset  96
    glm::vec4 AmbientColor;      // offset 112
    float     AmbientIntensity;  // offset 128
    float     DiffuseIntensity;  // offset 132
    float     EnvMapIntensity;   // offset 136
    float     ETA;               // offset 140
    float     FresnelPower;      // offset 144
    uint32_t  ScreenWidth;       // offset 148
    int32_t   DebugMode;         // offset 152
    int32_t   AreaLightsEnabled; // offset 156
    glm::mat4 ViewProjectionUnjittered; // offset 160, 64 bytes
    glm::mat4 PrevViewProjection;       // offset 224, 64 bytes
    float     TextureLodBias;           // offset 288
    float     ZNear;                    // offset 292
    float     ZFar;                     // offset 296
    uint32_t  ScreenHeight;             // offset 300
    glm::mat4 View;                     // offset 304 → total 368 bytes
};
static_assert(sizeof(SceneUBOData) == 368, "SceneUBOData size mismatch — check std140 layout");

struct ClusteredRendererStorage
{
    Ref<Shader>              OverrideShader;   // optional scene-wide override
    Ref<ShaderVariantSet>    MeshVariants;    // mesh.glsl named variants
    Ref<Shader>              NormalsShader;        // normals_debug.glsl (static)
    Ref<Shader>              NormalsSkinnedShader; // normals_debug.glsl + SKINNED
    Ref<Shader>              BoneWeightShader;     // bone_weight.glsl
    Ref<Shader>              DebugAABBShader;
    Ref<Shader>              DebugSphereShader;
    Ref<VertexArray>         DebugVAO;
    Ref<VertexBuffer>        DebugVBO;
    Ref<IndexBuffer>         DebugIBO;

    Ref<UniformBuffer> SceneUBO;             // binding 0 — SceneUBOData, uploaded once per frame

    // Clustered shading, brute force light culling
    Ref<StorageBuffer> LightBuffer;          // slot 1 — GPULight[MAX_LIGHTS]
    Ref<StorageBuffer> ClusterBuffer;        // slot 2 — Cluster[NUM_CLUSTERS]
    Ref<ComputeShader> ClusterBuildShader;
    Ref<ComputeShader> ClusterCullShader;

    // The grid depends only on these, so it is rebuilt only when one changes
    glm::mat4          GridInvProjection {0.f};
    uint32_t           GridW = 0;
    uint32_t           GridH = 0;
};

class ClusteredRenderer
{
public:
    // Must match CLUSTER_GRID_SIZE / MAX_LIGHTS_PER_CLUSTER in includes/types.glsl
    static constexpr uint32_t GRID_SIZE_X            = 12u;
    static constexpr uint32_t GRID_SIZE_Y            = 12u;
    static constexpr uint32_t GRID_SIZE_Z            = 24u;
    static constexpr uint32_t NUM_CLUSTERS           = GRID_SIZE_X * GRID_SIZE_Y * GRID_SIZE_Z;
    static constexpr uint32_t MAX_LIGHTS             = 1024u;
    static constexpr uint32_t MAX_LIGHTS_PER_CLUSTER = 100u;

    // Per-call scene state produced by BeginScene() and threaded explicitly into
    // DrawStaticMesh/DrawSkinnedMesh — avoids a shared global so nested scenes
    // (e.g. EnvironmentProbe::Bake's per-face BeginScene/EndScene) can't race
    // a future main-thread recording pass.
    struct SceneData
    {
        std::vector<Light> Lights;
        uint32_t EnvMapID         = 0;
        uint32_t IrradianceMapID  = 0;
        uint32_t PrefilteredMapID = 0;
        float    EnvMapIntensity = 0.f;
        float    ETA             = 0.667f;
        float    FresnelPower    = 5.f;
        Frustum  CameraFrustum  {};

        // Vulkan's DDGI output, imported as GL textures. probeNumRays == 0 or a missing
        // atlas falls back to the IBL/ambient indirect term.
        VulkanDDGIParams DDGI {};
        uint32_t DDGIIrradianceID = 0;
        uint32_t DDGIDistanceID   = 0;
    };

    static void Init();
    static void InitClustered();
    static void Shutdown();

    // renderW/renderH: the actual render-target dimensions (HDR FBO size).
    // Pass these explicitly when renderScale != 1.0 so ScreenWidth and the cluster
    // tile dimensions match gl_FragCoord — they default to frame.viewportWidth/Height.
    [[nodiscard]] static SceneData BeginScene(const RenderFrame& frame, CommandList& cmd,
                                               uint32_t renderW = 0, uint32_t renderH = 0);
    static void EndScene();

    static void SetOverrideShader(const Ref<Shader>& shader) { s_Data->OverrideShader = shader; }
    static void ClearOverrideShader()                        { s_Data->OverrideShader.reset(); }

    static uint32_t GetDrawCalls()    { return s_DrawCalls; }
    static uint64_t GetTriangles()    { return s_Triangles; }
    static uint32_t GetGroupsTotal()  { return s_GroupsTotal; }
    static uint32_t GetGroupsCulled() { return s_GroupsCulled; }

    static void DrawDebugLights(const std::vector<Light>& lights);
    // prevTransform feeds the velocity buffer; null means the mesh didn't move.
    static void DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform, CommandList& cmd, const SceneData& scene,
                                const glm::mat4* prevTransform = nullptr);
    static void DrawStaticMesh(StaticMesh& mesh,  const glm::mat4& transform, CommandList& cmd, const SceneData& scene,
                                const glm::mat4* prevTransform = nullptr);

    static void ReloadVariants() { if (s_Data->MeshVariants) s_Data->MeshVariants->ReloadAll(); }

private:
    static void CullLights(const RenderFrame& frame, CommandList& cmd,
                           uint32_t renderW, uint32_t renderH, float zNear, float zFar);
    static void BuildClusters(const glm::mat4& invProj, uint32_t w, uint32_t h, float zNear, float zFar);

    static constexpr uint32_t MAX_POINT_LIGHTS_SKINNED = 16u;

    static ClusteredRendererStorage* s_Data;
    static uint32_t           s_DrawCalls;
    static uint64_t           s_Triangles;
    static uint32_t           s_GroupsTotal;
    static uint32_t           s_GroupsCulled;
};

} 
