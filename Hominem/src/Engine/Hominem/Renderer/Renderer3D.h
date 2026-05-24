#pragma once

#include "Shader.h"
#include "Buffer.h"
#include "VertexArray.h"
#include "StorageBuffer.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Renderer/Frustum.h"
#include "Hominem/Renderer/RenderFrame.h"

namespace Hominem {

struct MeshRendererComponent
{
    Ref<SkinnedMesh> Mesh;
    Ref<Shader>      Shader;
};

struct Renderer3DStorage
{
    Ref<ShaderLibrary> ShaderLibrary;
    Ref<Shader>        DefaultShader;
    Ref<Shader>        OverrideShader;       // optional scene-wide override
    Ref<Shader>        StaticMeshShader;     // forward_plus.glsl — set by InitForwardPlus
    Ref<Shader>        SkinnedMeshShader;
    Ref<Shader>        NormalsShader;
    Ref<Shader>        NormalsSkinnedShader;
    Ref<Shader>        DebugAABBShader;
    Ref<Shader>        DebugSphereShader;
    Ref<VertexArray>   DebugVAO;
    Ref<VertexBuffer>  DebugVBO;
    Ref<IndexBuffer>   DebugIBO;

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

    static void Init();
    static void InitForwardPlus();
    static void Shutdown();

    static void BeginScene(const RenderFrame& frame);
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

    static void DrawDebugPointLights(const std::vector<PointLight>& lights);
    static void DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform);
    static void DrawStaticMesh(StaticMesh& mesh,  const glm::mat4& transform);
    static void Draw(const MeshRendererComponent& rc, const glm::mat4& transform);

    static Ref<ShaderLibrary> GetShaderLibrary() { return s_Data->ShaderLibrary; }

private:
    static void CullLights(const RenderFrame& frame);
    static void ResizeTileBuffers(uint32_t w, uint32_t h);

    static constexpr uint32_t MAX_POINT_LIGHTS_SKINNED = 16u;

    struct SceneData
    {
        glm::mat4        ViewProjection{};
        glm::vec3        CameraWorldPos{};
        DirectionalLight Light;
        uint32_t         ViewportWidth = 0;
        Shader*          BoundShader   = nullptr;
        std::vector<PointLight> PointLights;
    };

    static Renderer3DStorage* s_Data;
    static SceneData*         s_Scene;
    static bool               s_DrawNormals;
    static float              s_NormalLength;
    static bool               s_DrawAABB;
    static bool               s_DebugHeatmap;
};

} 
