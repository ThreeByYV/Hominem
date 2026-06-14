#pragma once

#include "Shader.h"
#include "Texture.h"
#include "ShaderPermutation.h"
#include "Buffer.h"
#include "VertexArray.h"
#include "StorageBuffer.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Renderer/Frustum.h"
#include "Hominem/Renderer/RenderFrame.h"

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
    float     _pad;              // offset 156 → total 160 bytes
};
static_assert(sizeof(SceneUBOData) == 160, "SceneUBOData size mismatch — check std140 layout");

struct Renderer3DStorage
{
    Ref<ShaderLibrary>       ShaderLibrary;
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

    Ref<Texture2D>     BRDFLUT;              // slot 6, RG16F split-sum (scale, bias) LUT, baked once at startup

    Ref<UniformBuffer> SceneUBO;             // binding 0 — SceneUBOData, uploaded once per frame

    // Forward+ tile-based light culling
    Ref<StorageBuffer> LightBuffer;          // slot 1 — GPUPointLight[MAX_LIGHTS]
    Ref<StorageBuffer> LightIndexList;       // slot 2 — flat uint index list, per-tile
    Ref<StorageBuffer> LightGrid;            // slot 3 — LightGridEntry[numTiles]
    Ref<StorageBuffer> GlobalLightCounter;   // slot 4 — atomic uint, reset each frame
    Ref<ComputeShader> LightCullingShader;
    uint32_t           NumTilesX = 0;
    uint32_t           NumTilesY = 0;
    uint32_t           ViewportW = 0;
    uint32_t           ViewportH = 0;
};

class Renderer3D
{
public:
    static constexpr uint32_t TILE_SIZE           = 16u;
    static constexpr uint32_t MAX_LIGHTS          = 1024u;
    static constexpr uint32_t MAX_LIGHTS_PER_TILE = 128u;

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
    };

    static void Init();
    static void InitForwardPlus();
    static void Shutdown();

    [[nodiscard]] static SceneData BeginScene(const RenderFrame& frame, CommandList& cmd);
    static void EndScene();

    static void SetOverrideShader(const Ref<Shader>& shader) { s_Data->OverrideShader = shader; }
    static void ClearOverrideShader()                        { s_Data->OverrideShader.reset(); }

    static void SetDrawNormals(bool v)      { s_DrawNormals = v; }
    static bool GetDrawNormals()            { return s_DrawNormals; }
    static void SetNormalLength(float l)    { s_NormalLength = l; }
    static void SetDrawAABB(bool v)         { s_DrawAABB = v; }
    static bool GetDrawAABB()               { return s_DrawAABB; }
    static void SetDebugHeatmap(bool v)     { s_DebugHeatmap = v; }
    static bool GetDebugHeatmap()           { return s_DebugHeatmap; }
    static void SetDrawBoneWeights(bool v)   { s_DrawBoneWeights = v; }
    static bool GetDrawBoneWeights()         { return s_DrawBoneWeights; }
    static void SetToonShading(bool v)       { s_ToonShading = v; }
    static bool GetToonShading()             { return s_ToonShading; }
    static void SetDisplayBoneIndex(int i)  { s_DisplayBoneIndex = i; }
    static int  GetDisplayBoneIndex()       { return s_DisplayBoneIndex; }

    // Returns lower value for low-end integrated GPUs, 1.0 otherwise.
    static float GetRecommendedRenderScale() { return s_RecommendedRenderScale; }

    static uint32_t GetDrawCalls()    { return s_DrawCalls; }
    static uint64_t GetTriangles()    { return s_Triangles; }
    static uint32_t GetGroupsTotal()  { return s_GroupsTotal; }
    static uint32_t GetGroupsCulled() { return s_GroupsCulled; }

    static void DrawDebugLights(const std::vector<Light>& lights);
    static void DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform, CommandList& cmd, const SceneData& scene);
    static void DrawStaticMesh(StaticMesh& mesh,  const glm::mat4& transform, CommandList& cmd, const SceneData& scene);

    static Ref<ShaderLibrary> GetShaderLibrary() { return s_Data->ShaderLibrary; }
    static void ReloadVariants() { if (s_Data->MeshVariants) s_Data->MeshVariants->ReloadAll(); }

private:
    static void CullLights(const RenderFrame& frame, CommandList& cmd);
    static void ResizeTileBuffers(uint32_t w, uint32_t h);

    static constexpr uint32_t MAX_POINT_LIGHTS_SKINNED = 16u;

    static Renderer3DStorage* s_Data;
    static bool               s_DrawNormals;
    static float              s_NormalLength;
    static bool               s_DrawAABB;
    static bool               s_DrawBoneWeights;
    static int                s_DisplayBoneIndex;
    static bool               s_ToonShading;
    static bool               s_DebugHeatmap;
    static float              s_RecommendedRenderScale;
    static uint32_t           s_DrawCalls;
    static uint64_t           s_Triangles;
    static uint32_t           s_GroupsTotal;
    static uint32_t           s_GroupsCulled;
};

} 
