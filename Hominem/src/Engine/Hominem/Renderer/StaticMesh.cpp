#include "hmnpch.h"
#include "StaticMesh.h"
#include "RenderThread.h"

#include <glad/glad.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <meshoptimizer.h>

#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Hominem
{
    // Returns a human-readable size string in meters, cm, and feet+inches.
    // e.g. 1.727 → "1.73 m / 172.7 cm / 5'8\""
    static std::string FormatSize(float meters)
    {
        int   totalInches = static_cast<int>(std::round(meters * 39.3701f));
        int   feet        = totalInches / 12;
        int   inches      = totalInches % 12;
        char  buf[64];
        std::snprintf(buf, sizeof(buf), "%.2f m / %.1f cm / %d'%d\"",
                      meters, meters * 100.f, feet, inches);
        return buf;
    }

    static constexpr uint32_t k_CacheMagic   = 0x48534D53u; // "SMSH"
    static constexpr uint32_t k_CacheVersion = 4u;          // v4: PBR — albedo + metalRoughness per group

    constexpr unsigned int k_AssimpFlags =
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs |
            aiProcess_JoinIdenticalVertices |
            aiProcess_OptimizeMeshes |
            aiProcess_PreTransformVertices; // bakes node hierarchy into vertex positions

    static Ref<Texture2D> s_WhiteTexture;
    static Ref<Texture2D> s_DefaultMetalRoughness; // G=0.5 roughness, B=0 metalness

    static Ref<Texture2D> GetWhiteTexture()
    {
        if (!s_WhiteTexture)
        {
            s_WhiteTexture = Texture2D::Create(1, 1, TextureFormat::RGBA8);
            uint32_t white = 0xFFFFFFFFu;

            s_WhiteTexture->SetData(&white, sizeof(white));
            s_WhiteTexture->QueueUpload();
        }
        return s_WhiteTexture;
    }

    static Ref<Texture2D> GetDefaultMetalRoughness()
    {
        if (!s_DefaultMetalRoughness)
        {
            s_DefaultMetalRoughness = Texture2D::Create(1, 1, TextureFormat::RGBA8);
            // R=0, G=128 (roughness=0.5), B=0 (metalness=0), A=255
            uint8_t pixel[4] = { 0, 128, 0, 255 };
            uint32_t packed; memcpy(&packed, pixel, 4);
            s_DefaultMetalRoughness->SetData(&packed, sizeof(packed));
            s_DefaultMetalRoughness->QueueUpload();
        }
        return s_DefaultMetalRoughness;
    }

    StaticMesh::~StaticMesh()
    {
        if (m_VAO)
        {
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
        if (m_VBO)
        {
            glDeleteBuffers(1, &m_VBO);
            m_VBO = 0;
        }
        if (m_IBO)
        {
            glDeleteBuffers(1, &m_IBO);
            m_IBO = 0;
        }
    }

    //todo: again lets gave some file i/o thing and use that here in FileUtils.h
    bool StaticMesh::LoadFromFile(const std::string &path)
    {
        // GLB/glTF have embedded textures that can't be re-resolved from a path cache.
        bool canCache = !path.ends_with(".glb") && !path.ends_with(".gltf")
                     && !path.ends_with(".GLB") && !path.ends_with(".GLTF");

        if (canCache)
        {
            std::string binPath = path + ".bin";
            if (std::filesystem::exists(binPath))
            {
                HMN_CORE_INFO("StaticMesh: loading from cache '{}'", binPath);
                if (LoadBinary(binPath))
                    return true;
                HMN_CORE_WARN("StaticMesh: cache corrupt, re-importing '{}'", path);
            }
        }

        return LoadAssimp(path);
    }

    bool StaticMesh::LoadBinary(const std::string &binPath)
    {
        std::ifstream f(binPath, std::ios::binary);
        if (!f) return false;

        struct Header
        {
            uint32_t magic, version, vertCount, idxCount, groupCount;
        } hdr;
        f.read(reinterpret_cast<char *>(&hdr), sizeof(hdr));

        if (!f || hdr.magic != k_CacheMagic || hdr.version != k_CacheVersion)
            return false;

        std::vector<StaticVertex> verts(hdr.vertCount);
        std::vector<uint32_t> indices(hdr.idxCount);

        f.read(reinterpret_cast<char *>(verts.data()), hdr.vertCount * sizeof(StaticVertex));
        f.read(reinterpret_cast<char *>(indices.data()), hdr.idxCount * sizeof(uint32_t));

        m_DrawGroups.clear();
        m_DrawGroups.reserve(hdr.groupCount);

        for (uint32_t i = 0; i < hdr.groupCount; i++)
        {
            uint32_t offset, count, pathLen;
            int32_t baseVertex;

            f.read(reinterpret_cast<char *>(&offset), sizeof(uint32_t));
            f.read(reinterpret_cast<char *>(&count), sizeof(uint32_t));
            f.read(reinterpret_cast<char *>(&baseVertex), sizeof(int32_t));
            f.read(reinterpret_cast<char *>(&pathLen), sizeof(uint32_t));

            auto readTex = [&]() -> Ref<Texture2D>
            {
                uint32_t len = 0;
                f.read(reinterpret_cast<char*>(&len), sizeof(uint32_t));
                if (len == 0) return nullptr;
                std::string texPath(len, '\0');
                f.read(texPath.data(), len);
                if (texPath.size() > 6 && texPath.substr(0, 6) == "color:")
                {
                    unsigned r, g, b, a;
                    sscanf(texPath.c_str() + 6, "%02X%02X%02X%02X", &r, &g, &b, &a);
                    uint8_t pixel[4] = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
                    uint32_t packed; memcpy(&packed, pixel, 4);
                    auto t = Texture2D::Create(1, 1, TextureFormat::RGBA8);
                    t->SetData(&packed, 4);
                    t->QueueUpload();
                    return t;
                }
                return Texture2D::Create(texPath);
            };

            // Old format stored pathLen inline; new format uses readTex() which reads its own length.
            // We already read pathLen above — read the string then read mrPath via readTex().
            Ref<Texture2D> albedo;
            if (pathLen > 0)
            {
                std::string texPath(pathLen, '\0');
                f.read(texPath.data(), pathLen);
                if (texPath.size() > 6 && texPath.substr(0, 6) == "color:")
                {
                    unsigned r, g, b, a;
                    sscanf(texPath.c_str() + 6, "%02X%02X%02X%02X", &r, &g, &b, &a);
                    uint8_t pixel[4] = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
                    uint32_t packed; memcpy(&packed, pixel, 4);
                    albedo = Texture2D::Create(1, 1, TextureFormat::RGBA8);
                    albedo->SetData(&packed, 4);
                    albedo->QueueUpload();
                }
                else
                {
                    albedo = Texture2D::Create(texPath);
                }
            }
            Ref<Texture2D> mr = readTex(); // metalRoughness — new in v4

            m_DrawGroups.push_back({
                albedo ? albedo : GetWhiteTexture(),
                mr     ? mr     : GetDefaultMetalRoughness(),
                offset, count, baseVertex});
        }

        if (!f) return false;

        m_AABBMin = glm::vec3(FLT_MAX);
        m_AABBMax = glm::vec3(-FLT_MAX);
        for (const auto& v : verts)
        {
            m_AABBMin = glm::min(m_AABBMin, v.Position);
            m_AABBMax = glm::max(m_AABBMax, v.Position);
        }

        m_PendingVerts   = verts;
        m_PendingIndices = indices;
        {
            glm::vec3 size = m_AABBMax - m_AABBMin;
            HMN_CORE_INFO("StaticMesh: cache loaded — {}v {}i {}groups",
                          hdr.vertCount, hdr.idxCount, hdr.groupCount);
            HMN_CORE_INFO("StaticMesh: size  W:{} H:{} D:{}",
                          FormatSize(size.x), FormatSize(size.y), FormatSize(size.z));
        }
        RenderThread::QueueUpload([this] {
            Upload(m_PendingVerts, m_PendingIndices);
            m_PendingVerts  = {};
            m_PendingIndices = {};
        });
        return true;
    }

    //todo: lets move this one and SkinnedMesh Assimp things into some importer class, too much here
    bool StaticMesh::LoadAssimp(const std::string &path)
    {
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(path, k_AssimpFlags);

        if (!scene || !scene->mRootNode)
        {
            HMN_CORE_ERROR("StaticMesh: assimp failed '{}': {}", path, importer.GetErrorString());
            return false;
        }

        std::string dir = path.substr(0, path.find_last_of("/\\"));

        // --- count totals ---
        uint32_t totalVerts = 0, totalIndices = 0;
        for (uint32_t i = 0; i < scene->mNumMeshes; i++)
        {
            totalVerts += scene->mMeshes[i]->mNumVertices;
            totalIndices += scene->mMeshes[i]->mNumFaces * 3;
        }

        // --- detect unit scale ---
        // FBX stores UnitScaleFactor in scene metadata (Blender default = 1.0 → cm).
        // Multiply by 0.01 to convert cm → metres so 1 engine unit = 1 metre.
        // glTF is always metres (no metadata key), OBJ has no standard → both default 1.0.
        float scaleToMetres = 1.0f;
        if (scene->mMetaData)
        {
            double unitScale = 1.0;
            if (scene->mMetaData->Get("UnitScaleFactor", unitScale))
                scaleToMetres = static_cast<float>(unitScale) * 0.01f;
        }
        HMN_CORE_INFO("StaticMesh: unit scale {:.4f} (1 file unit = {:.4f} m)",
                      scaleToMetres, scaleToMetres);

        std::vector<StaticVertex> verts;
        std::vector<uint32_t> indices;
        verts.reserve(totalVerts);
        indices.reserve(totalIndices);

        // --- extract geometry ---
        struct RawSub
        {
            uint32_t idxOffset, idxCount;
            int32_t baseVertex;
            uint32_t matIdx;
        };
        std::vector<RawSub> rawSubs;
        rawSubs.reserve(scene->mNumMeshes);

        const aiVector3D kZero(0.f);

        for (uint32_t m = 0; m < scene->mNumMeshes; m++)
        {
            const aiMesh *mesh = scene->mMeshes[m];
            int32_t baseVertex = static_cast<int32_t>(verts.size());
            uint32_t baseIndex = static_cast<uint32_t>(indices.size());
            uint32_t faceIndices = 0;

            for (uint32_t v = 0; v < mesh->mNumVertices; v++)
            {
                const auto &p = mesh->mVertices[v];
                const auto &n = mesh->mNormals ? mesh->mNormals[v] : kZero;
                const auto &u = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][v] : kZero;
                verts.push_back({
                    { p.x * scaleToMetres, p.y * scaleToMetres, p.z * scaleToMetres },
                    { n.x, n.y, n.z },
                    { u.x, u.y }
                });
            }

            for (uint32_t f = 0; f < mesh->mNumFaces; f++)
            {
                const aiFace &face = mesh->mFaces[f];
                if (face.mNumIndices != 3) continue;
                indices.push_back(face.mIndices[0]);
                indices.push_back(face.mIndices[1]);
                indices.push_back(face.mIndices[2]);
                faceIndices += 3;
            }

            rawSubs.push_back({baseIndex, faceIndices, baseVertex, mesh->mMaterialIndex});
        }

        // --- load materials ---
        std::vector<Ref<Texture2D>> matAlbedo(scene->mNumMaterials);
        std::vector<Ref<Texture2D>> matMetalRoughness(scene->mNumMaterials);
        std::vector<std::string>    matAlbedoPaths(scene->mNumMaterials);
        std::vector<std::string>    matMRPaths(scene->mNumMaterials);

        // Keep old names as aliases so the existing resolve/makeColor lambdas below still compile
        auto& matTextures = matAlbedo;
        auto& matPaths    = matAlbedoPaths;

        // Resolve a texture path from the FBX — tries multiple candidates since FBX files
        // often store absolute artist-machine paths that won't exist on another machine.
        auto resolveTexture = [&](const std::string &rawPath) -> std::string
        {
            std::string norm = rawPath;
            std::replace(norm.begin(), norm.end(), '\\', '/');

            std::string filename = norm.substr(norm.find_last_of("/") + 1);

            // Build extension alternatives (.jpg <-> .jpeg, etc.)
            auto withAltExt = [](const std::string &p) -> std::string
            {
                auto dot = p.rfind('.');
                if (dot == std::string::npos) return "";
                std::string ext = p.substr(dot + 1);
                if (ext == "jpg") return p.substr(0, dot + 1) + "jpeg";
                else if (ext == "jpeg") return p.substr(0, dot + 1) + "jpg";
                return "";
            };

            std::vector<std::string> bases = {
                norm, // absolute or correct relative
                dir + "/" + norm, // relative to FBX dir
                dir + "/" + filename, // filename only, next to FBX
                dir + "/textures/" + filename, // textures/ sibling of FBX
                dir + "/../textures/" + filename, // textures/ one level up from FBX dir
            };

            for (const auto &b: bases)
            {
                if (std::filesystem::exists(b))
                    return std::filesystem::path(b).lexically_normal().string();
                std::string alt = withAltExt(b);
                if (!alt.empty() && std::filesystem::exists(alt))
                    return std::filesystem::path(alt).lexically_normal().string();
            }

            HMN_CORE_WARN("StaticMesh: could not resolve texture '{}' — tried:", rawPath);
            for (const auto &b: bases)
                HMN_CORE_WARN("  {}", b);
            return "";
        };

        auto makeColorTex = [](aiColor4D c) -> Ref<Texture2D>
        {
            uint8_t pixel[4] = {
                (uint8_t)(glm::clamp(c.r, 0.f, 1.f) * 255),
                (uint8_t)(glm::clamp(c.g, 0.f, 1.f) * 255),
                (uint8_t)(glm::clamp(c.b, 0.f, 1.f) * 255),
                (uint8_t)(glm::clamp(c.a, 0.f, 1.f) * 255)
            };
            uint32_t packed;
            memcpy(&packed, pixel, 4);
            auto tex = Texture2D::Create(1, 1, TextureFormat::RGBA8);
            tex->SetData(&packed, 4);
            tex->QueueUpload();
            return tex;
        };

        // Resolve embedded GLB texture — path is "*N" where N indexes scene->mTextures.
        auto resolveEmbedded = [&](const std::string& rawPath) -> Ref<Texture2D>
        {
            if (rawPath.empty() || rawPath[0] != '*') return nullptr;
            int idx = std::atoi(rawPath.c_str() + 1);
            if (idx < 0 || idx >= (int)scene->mNumTextures) return nullptr;

            const aiTexture* emb = scene->mTextures[idx];
            if (emb->mHeight == 0)
            {
                // Compressed blob (PNG/JPG) — mWidth holds byte count
                return Texture2D::CreateFromMemory(
                    reinterpret_cast<const uint8_t*>(emb->pcData), emb->mWidth);
            }
            else
            {
                // Raw ARGB8888 uncompressed — convert to RGBA
                uint32_t numPixels = emb->mWidth * emb->mHeight;
                std::vector<uint8_t> rgba(numPixels * 4);
                for (uint32_t p = 0; p < numPixels; p++)
                {
                    rgba[p*4+0] = emb->pcData[p].r;
                    rgba[p*4+1] = emb->pcData[p].g;
                    rgba[p*4+2] = emb->pcData[p].b;
                    rgba[p*4+3] = emb->pcData[p].a;
                }
                auto tex = Texture2D::Create(emb->mWidth, emb->mHeight, TextureFormat::RGBA8);
                tex->SetData(rgba.data(), (uint32_t)rgba.size());
                tex->QueueUpload();
                return tex;
            }
        };

        // Try to find a base-color texture from a material, checking both glTF (BASE_COLOR)
        // and legacy (DIFFUSE) slots so the same code works for both file formats.
        auto findBaseColor = [&](const aiMaterial* mat, aiString& outPath) -> bool
        {
            if (mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
                return mat->GetTexture(aiTextureType_BASE_COLOR, 0, &outPath) == AI_SUCCESS;
            if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
                return mat->GetTexture(aiTextureType_DIFFUSE, 0, &outPath) == AI_SUCCESS;
            return false;
        };

        for (uint32_t i = 0; i < scene->mNumMaterials; i++)
        {
            const aiMaterial *mat = scene->mMaterials[i];

            aiString aiTexPath;
            if (findBaseColor(mat, aiTexPath))
            {
                std::string raw = aiTexPath.data;

                // Embedded GLB texture
                if (!raw.empty() && raw[0] == '*')
                {
                    matPaths[i]    = raw;
                    matTextures[i] = resolveEmbedded(raw);
                    if (matTextures[i])
                        HMN_CORE_INFO("StaticMesh: embedded texture [{}] decoded", i);
                    else
                        HMN_CORE_WARN("StaticMesh: failed to decode embedded texture [{}]", i);
                    continue;
                }

                // External file
                std::string resolved = resolveTexture(raw);
                if (!resolved.empty())
                {
                    matPaths[i]    = resolved;
                    matTextures[i] = Texture2D::Create(resolved);
                    if (matTextures[i])
                        HMN_CORE_INFO("StaticMesh: texture [{}] loaded '{}'", i, resolved);
                    else
                        HMN_CORE_WARN("StaticMesh: Texture2D::Create failed '{}'", resolved);
                    continue;
                }
            }

            // No texture — fall back to material base color as a 1×1 solid
            aiColor4D diffuse(0.7f, 0.7f, 0.7f, 1.f);
            aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &diffuse);
            if (diffuse.r == 0 && diffuse.g == 0 && diffuse.b == 0)
                aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse);

            char buf[20];
            snprintf(buf, sizeof(buf), "color:%02X%02X%02X%02X",
                (uint8_t)(diffuse.r * 255), (uint8_t)(diffuse.g * 255),
                (uint8_t)(diffuse.b * 255), (uint8_t)(diffuse.a * 255));

            matPaths[i]    = buf;
            matTextures[i] = makeColorTex(diffuse);
            HMN_CORE_INFO("StaticMesh: material [{}] color fallback {}", i, buf);

            // MetalRoughness — glTF packs roughness in G, metalness in B
            aiString mrPath;
            if (mat->GetTextureCount(aiTextureType_METALNESS) > 0 &&
                mat->GetTexture(aiTextureType_METALNESS, 0, &mrPath) == AI_SUCCESS)
            {
                std::string raw = mrPath.data;
                if (!raw.empty() && raw[0] == '*')
                {
                    matMetalRoughness[i] = resolveEmbedded(raw);
                    matMRPaths[i]        = raw;
                    if (matMetalRoughness[i])
                        HMN_CORE_INFO("StaticMesh: metalRoughness [{}] embedded", i);
                }
                else
                {
                    std::string resolved = resolveTexture(raw);
                    if (!resolved.empty())
                    {
                        matMetalRoughness[i] = Texture2D::Create(resolved);
                        matMRPaths[i]        = resolved;
                        HMN_CORE_INFO("StaticMesh: metalRoughness [{}] '{}'", i, resolved);
                    }
                }
            }
        }

        // --- sort by material to minimise texture rebinds ---
        std::sort(rawSubs.begin(), rawSubs.end(),
                  [](const RawSub &a, const RawSub &b) { return a.matIdx < b.matIdx; });

        // --- build draw groups ---
        m_DrawGroups.clear();
        std::vector<std::string> groupAlbedoPaths2;
        std::vector<std::string> groupMRPaths2;
        m_DrawGroups.reserve(rawSubs.size());
        groupAlbedoPaths2.reserve(rawSubs.size());
        groupMRPaths2.reserve(rawSubs.size());

        for (const auto &sub: rawSubs)
        {
            auto albedo = (sub.matIdx < matAlbedo.size() && matAlbedo[sub.matIdx])
                           ? matAlbedo[sub.matIdx]
                           : GetWhiteTexture();
            auto mr     = (sub.matIdx < matMetalRoughness.size() && matMetalRoughness[sub.matIdx])
                           ? matMetalRoughness[sub.matIdx]
                           : GetDefaultMetalRoughness();

            const std::string& albedoPath = sub.matIdx < matAlbedoPaths.size() ? matAlbedoPaths[sub.matIdx] : "";
            const std::string& mrPath     = sub.matIdx < matMRPaths.size()     ? matMRPaths[sub.matIdx]     : "";

            m_DrawGroups.push_back({albedo, mr, sub.idxOffset * (uint32_t)sizeof(uint32_t), sub.idxCount, sub.baseVertex});
            groupAlbedoPaths2.push_back(albedoPath);
            groupMRPaths2.push_back(mrPath);
        }

        OptimizeGeometry(verts, indices, m_DrawGroups);

        m_AABBMin = glm::vec3(FLT_MAX);
        m_AABBMax = glm::vec3(-FLT_MAX);
        for (const auto &v: verts)
        {
            m_AABBMin = glm::min(m_AABBMin, v.Position);
            m_AABBMax = glm::max(m_AABBMax, v.Position);
        }
        {
            glm::vec3 size = m_AABBMax - m_AABBMin;
            HMN_CORE_INFO("StaticMesh: '{}' — {}v {}i {}groups", path, verts.size(), indices.size(), rawSubs.size());
            HMN_CORE_INFO("StaticMesh: size  W:{} H:{} D:{}",
                          FormatSize(size.x), FormatSize(size.y), FormatSize(size.z));
        }

        if (!path.ends_with(".glb") && !path.ends_with(".gltf")
         && !path.ends_with(".GLB") && !path.ends_with(".GLTF"))
            WriteBinary(path + ".bin", verts, indices, m_DrawGroups, groupAlbedoPaths2, groupMRPaths2);
        m_PendingVerts   = verts;
        m_PendingIndices = indices;
        RenderThread::QueueUpload([this] {
            Upload(m_PendingVerts, m_PendingIndices);
            m_PendingVerts  = {};
            m_PendingIndices = {};
        });
        return true;
    }

    void StaticMesh::OptimizeGeometry(std::vector<StaticVertex>& verts,
                                      std::vector<uint32_t>&     indices,
                                      std::vector<DrawGroup>&    groups)
    {
        std::vector<StaticVertex> newVerts;
        std::vector<uint32_t>     newIndices;
        newVerts.reserve(verts.size());
        newIndices.reserve(indices.size());

        for (auto& group : groups)
        {
            const uint32_t localIdxCount  = group.IndexCount;
            const uint32_t localIdxOffset = group.IndexByteOffset / sizeof(uint32_t);
            const int32_t  baseVertex     = group.BaseVertex;

            // Find how many vertices this group actually uses
            uint32_t maxIdx = 0;
            for (uint32_t i = localIdxOffset; i < localIdxOffset + localIdxCount; i++)
                maxIdx = std::max(maxIdx, indices[i]);
            const uint32_t localVertCount = maxIdx + 1;

            const StaticVertex* srcVerts   = &verts[baseVertex];
            const uint32_t*     srcIndices = &indices[localIdxOffset];

            // 1. Generate vertex remap — deduplicates identical vertices
            std::vector<uint32_t> remap(localVertCount);
            size_t uniqueVertCount = meshopt_generateVertexRemap(
                remap.data(),
                srcIndices,   localIdxCount,
                srcVerts,     localVertCount, sizeof(StaticVertex));

            std::vector<uint32_t>     optIndices(localIdxCount);
            std::vector<StaticVertex> optVerts(uniqueVertCount);

            meshopt_remapIndexBuffer (optIndices.data(), srcIndices, localIdxCount, remap.data());
            meshopt_remapVertexBuffer(optVerts.data(),   srcVerts,   localVertCount, sizeof(StaticVertex), remap.data());

            // 2. Vertex cache optimisation (reduces GPU vertex shader invocations)
            meshopt_optimizeVertexCache(optIndices.data(), optIndices.data(),
                                        localIdxCount, uniqueVertCount);

            // 3. Overdraw optimisation (reduces pixel shader invocations from overdraw)
            meshopt_optimizeOverdraw(optIndices.data(), optIndices.data(), localIdxCount,
                                     &optVerts[0].Position.x, uniqueVertCount,
                                     sizeof(StaticVertex), 1.05f);

            // 4. Vertex fetch optimisation — reorders vertices to match index access order,
            //    minimising cache misses when the GPU fetches vertex data.
            //    Also generates a smaller vertex buffer when vertices are unused after simplify.
            meshopt_optimizeVertexFetch(optVerts.data(), optIndices.data(), localIdxCount,
                                        optVerts.data(), uniqueVertCount, sizeof(StaticVertex));

            // Update group to point at the new packed location
            group.BaseVertex      = static_cast<int32_t>(newVerts.size());
            group.IndexByteOffset = static_cast<uint32_t>(newIndices.size() * sizeof(uint32_t));
            group.IndexCount      = localIdxCount;

            newVerts.insert  (newVerts.end(),   optVerts.begin(),   optVerts.end());
            newIndices.insert(newIndices.end(), optIndices.begin(), optIndices.end());
        }

        HMN_CORE_INFO("StaticMesh: meshopt {} -> {}v, {} -> {}i",
                      verts.size(), newVerts.size(), indices.size(), newIndices.size());

        verts   = std::move(newVerts);
        indices = std::move(newIndices);
    }

    //todo: move to some file io part of the application
    void StaticMesh::WriteBinary(const std::string &binPath,
                                 const std::vector<StaticVertex> &verts,
                                 const std::vector<uint32_t> &indices,
                                 const std::vector<DrawGroup> &groups,
                                 const std::vector<std::string> &groupAlbedoPaths,
                                 const std::vector<std::string> &groupMRPaths)
    {
        std::ofstream f(binPath, std::ios::binary);
        if (!f)
        {
            HMN_CORE_WARN("StaticMesh: cannot write cache '{}'", binPath);
            return;
        }

        struct Header
        {
            uint32_t magic, version, vertCount, idxCount, groupCount;
        } hdr = {
                    k_CacheMagic, k_CacheVersion,
                    (uint32_t) verts.size(), (uint32_t) indices.size(), (uint32_t) groups.size()
                };

        f.write(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
        f.write(reinterpret_cast<const char *>(verts.data()), verts.size() * sizeof(StaticVertex));
        f.write(reinterpret_cast<const char *>(indices.data()), indices.size() * sizeof(uint32_t));

        auto writePath = [&](const std::string& p)
        {
            uint32_t len = static_cast<uint32_t>(p.size());
            f.write(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
            if (len > 0) f.write(p.data(), len);
        };

        for (size_t i = 0; i < groups.size(); i++)
        {
            const auto& g          = groups[i];
            const std::string& albedo = i < groupAlbedoPaths.size() ? groupAlbedoPaths[i] : "";
            const std::string& mr     = i < groupMRPaths.size()     ? groupMRPaths[i]     : "";

            f.write(reinterpret_cast<const char*>(&g.IndexByteOffset), sizeof(uint32_t));
            f.write(reinterpret_cast<const char*>(&g.IndexCount),      sizeof(uint32_t));
            f.write(reinterpret_cast<const char*>(&g.BaseVertex),      sizeof(int32_t));
            writePath(albedo);
            writePath(mr);
        }

        HMN_CORE_INFO("StaticMesh: wrote cache '{}'", binPath);
    }

    void StaticMesh::Upload(const std::vector<StaticVertex> &verts, const std::vector<uint32_t> &indices)
    {
        if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); }
        if (m_VBO) { glDeleteBuffers(1, &m_VBO); }
        if (m_IBO) { glDeleteBuffers(1, &m_IBO); }

        glCreateVertexArrays(1, &m_VAO);
        glCreateBuffers(1, &m_VBO);
        glCreateBuffers(1, &m_IBO);

        glNamedBufferData(m_VBO, (GLsizeiptr) (verts.size() * sizeof(StaticVertex)), verts.data(), GL_STATIC_DRAW);
        glNamedBufferData(m_IBO, (GLsizeiptr) (indices.size() * sizeof(uint32_t)), indices.data(), GL_STATIC_DRAW);

        // Interleaved vertex buffer bound to binding point 0
        glVertexArrayVertexBuffer(m_VAO, 0, m_VBO, 0, sizeof(StaticVertex));
        glVertexArrayElementBuffer(m_VAO, m_IBO);

        // Position — location 0
        glEnableVertexArrayAttrib(m_VAO, 0);
        glVertexArrayAttribFormat(m_VAO, 0, 3, GL_FLOAT, GL_FALSE, (GLuint) offsetof(StaticVertex, Position));
        glVertexArrayAttribBinding(m_VAO, 0, 0);

        // Normal — location 1
        glEnableVertexArrayAttrib(m_VAO, 1);
        glVertexArrayAttribFormat(m_VAO, 1, 3, GL_FLOAT, GL_FALSE, (GLuint) offsetof(StaticVertex, Normal));
        glVertexArrayAttribBinding(m_VAO, 1, 0);

        // TexCoord — location 2
        glEnableVertexArrayAttrib(m_VAO, 2);
        glVertexArrayAttribFormat(m_VAO, 2, 2, GL_FLOAT, GL_FALSE, (GLuint) offsetof(StaticVertex, TexCoord));
        glVertexArrayAttribBinding(m_VAO, 2, 0);
    }

    void StaticMesh::Draw(const Ref<Shader> &shader, const glm::mat4 &transform)
    {
        if (!m_VAO || m_DrawGroups.empty()) return;

        // FBX winding order is not guaranteed — disable culling so both faces render
        glDisable(GL_CULL_FACE);

        shader->Bind();
        shader->SetMat4("u_Model", transform);
        shader->SetInt("u_Albedo",        0);
        shader->SetInt("u_MetalRoughness", 1);

        glBindVertexArray(m_VAO);

        for (const auto &group: m_DrawGroups)
        {
            (group.Albedo        ? group.Albedo        : GetWhiteTexture())->Bind(0);
            (group.MetalRoughness ? group.MetalRoughness : GetDefaultMetalRoughness())->Bind(1);

            glDrawElementsBaseVertex(
                GL_TRIANGLES,
                static_cast<GLsizei>(group.IndexCount),
                GL_UNSIGNED_INT,
                reinterpret_cast<void *>(static_cast<size_t>(group.IndexByteOffset)),
                group.BaseVertex);
        }

        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);
    }
}
