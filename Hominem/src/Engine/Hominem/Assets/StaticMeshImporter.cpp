#include "hmnpch.h"
#include "StaticMeshImporter.h"
#include "MaterialTextures.h"

#include "Hominem/Core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <meshoptimizer.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <filesystem>
#include <functional>

namespace Hominem {

namespace {

constexpr uint32_t k_CacheMagic   = 0x48534D53u; // "SMSH"
constexpr uint32_t k_CacheVersion = 7u;          // v7: vertices baked to metres via aiProcess_GlobalScale

constexpr unsigned int k_AssimpFlags =
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes |
        aiProcess_PreTransformVertices |
        aiProcess_CalcTangentSpace |
        aiProcess_GlobalScale; // FBX UnitScaleFactor -> metres (no-op for glTF/OBJ)

// "1.73 m / 172.7 cm / 5'8\"" for log readability.
std::string FormatSize(float meters)
{
    int totalInches = static_cast<int>(std::round(meters * 39.3701f));
    int feet        = totalInches / 12;
    int inches      = totalInches % 12;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f m / %.1f cm / %d'%d\"",
                  meters, meters * 100.f, feet, inches);
    return buf;
}

bool IsGltf(const std::string& path)
{
    return path.ends_with(".glb")  || path.ends_with(".gltf")
        || path.ends_with(".GLB")  || path.ends_with(".GLTF");
}

void ComputeWorldAABB(MeshData& data)
{
    data.AABBMin = glm::vec3( FLT_MAX);
    data.AABBMax = glm::vec3(-FLT_MAX);
    for (const auto& g : data.Groups)
    {
        for (int ci = 0; ci < 8; ci++)
        {
            glm::vec3 corner(
                (ci & 1) ? g.AABBMax.x : g.AABBMin.x,
                (ci & 2) ? g.AABBMax.y : g.AABBMin.y,
                (ci & 4) ? g.AABBMax.z : g.AABBMin.z);
            glm::vec3 wc = glm::vec3(g.NodeTransform * glm::vec4(corner, 1.f));
            data.AABBMin = glm::min(data.AABBMin, wc);
            data.AABBMax = glm::max(data.AABBMax, wc);
        }
    }
}

// Per-group meshopt: dedup, vertex-cache, overdraw, vertex-fetch. Repacks into one buffer.
void OptimizeGeometry(MeshData& data)
{
    std::vector<StaticVertex> newVerts;
    std::vector<uint32_t>     newIndices;
    newVerts.reserve(data.Vertices.size());
    newIndices.reserve(data.Indices.size());

    for (auto& group : data.Groups)
    {
        const uint32_t localIdxCount  = group.IndexCount;
        const uint32_t localIdxOffset = group.IndexByteOffset / sizeof(uint32_t);
        const int32_t  baseVertex     = group.BaseVertex;

        uint32_t maxIdx = 0;
        for (uint32_t i = localIdxOffset; i < localIdxOffset + localIdxCount; i++)
            maxIdx = std::max(maxIdx, data.Indices[i]);
        const uint32_t localVertCount = maxIdx + 1;

        const StaticVertex* srcVerts   = &data.Vertices[baseVertex];
        const uint32_t*     srcIndices = &data.Indices[localIdxOffset];

        std::vector<uint32_t> remap(localVertCount);
        size_t uniqueVertCount = meshopt_generateVertexRemap(
            remap.data(), srcIndices, localIdxCount,
            srcVerts, localVertCount, sizeof(StaticVertex));

        std::vector<uint32_t>     optIndices(localIdxCount);
        std::vector<StaticVertex> optVerts(uniqueVertCount);

        meshopt_remapIndexBuffer (optIndices.data(), srcIndices, localIdxCount, remap.data());
        meshopt_remapVertexBuffer(optVerts.data(), srcVerts, localVertCount, sizeof(StaticVertex), remap.data());

        meshopt_optimizeVertexCache(optIndices.data(), optIndices.data(), localIdxCount, uniqueVertCount);
        meshopt_optimizeOverdraw(optIndices.data(), optIndices.data(), localIdxCount,
                                 &optVerts[0].Position.x, uniqueVertCount, sizeof(StaticVertex), 1.05f);
        meshopt_optimizeVertexFetch(optVerts.data(), optIndices.data(), localIdxCount,
                                    optVerts.data(), uniqueVertCount, sizeof(StaticVertex));

        group.BaseVertex      = static_cast<int32_t>(newVerts.size());
        group.IndexByteOffset = static_cast<uint32_t>(newIndices.size() * sizeof(uint32_t));
        group.IndexCount      = localIdxCount;

        group.AABBMin = glm::vec3( FLT_MAX);
        group.AABBMax = glm::vec3(-FLT_MAX);
        for (const auto& v : optVerts)
        {
            group.AABBMin = glm::min(group.AABBMin, v.Position);
            group.AABBMax = glm::max(group.AABBMax, v.Position);
        }

        newVerts.insert  (newVerts.end(),   optVerts.begin(),   optVerts.end());
        newIndices.insert(newIndices.end(), optIndices.begin(), optIndices.end());
    }

    HMN_CORE_INFO("StaticMesh: meshopt {} -> {}v, {} -> {}i",
                  data.Vertices.size(), newVerts.size(), data.Indices.size(), newIndices.size());

    data.Vertices = std::move(newVerts);
    data.Indices  = std::move(newIndices);
}

Ref<Texture2D> ReadCachedTex(std::ifstream& f)
{
    uint32_t len = 0;
    f.read(reinterpret_cast<char*>(&len), sizeof(uint32_t));
    if (len == 0) return nullptr;
    std::string src(len, '\0');
    f.read(src.data(), len);
    if (src.size() > 6 && src.substr(0, 6) == "color:")
    {
        unsigned r, g, b, a;
        sscanf(src.c_str() + 6, "%02X%02X%02X%02X", &r, &g, &b, &a);
        uint8_t pixel[4] = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
        uint32_t packed; memcpy(&packed, pixel, 4);
        auto t = Texture2D::Create(1, 1, TextureFormat::RGBA8);
        t->SetData(&packed, 4);
        t->QueueUpload();
        return t;
    }
    return Texture2D::Create(src);
}

bool ReadCache(const std::string& binPath, MeshData& data)
{
    std::ifstream f(binPath, std::ios::binary);
    if (!f) return false;

    struct Header { uint32_t magic, version, vertCount, idxCount, groupCount; } hdr;
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!f || hdr.magic != k_CacheMagic || hdr.version != k_CacheVersion)
        return false;

    data.Vertices.resize(hdr.vertCount);
    data.Indices.resize(hdr.idxCount);
    f.read(reinterpret_cast<char*>(data.Vertices.data()), hdr.vertCount * sizeof(StaticVertex));
    f.read(reinterpret_cast<char*>(data.Indices.data()),  hdr.idxCount  * sizeof(uint32_t));

    data.Groups.clear();
    data.Groups.reserve(hdr.groupCount);
    for (uint32_t i = 0; i < hdr.groupCount; i++)
    {
        uint32_t offset, count, pathLen;
        int32_t  baseVertex;
        f.read(reinterpret_cast<char*>(&offset),     sizeof(uint32_t));
        f.read(reinterpret_cast<char*>(&count),      sizeof(uint32_t));
        f.read(reinterpret_cast<char*>(&baseVertex), sizeof(int32_t));

        glm::mat4 nodeTransform(1.f);
        f.read(reinterpret_cast<char*>(glm::value_ptr(nodeTransform)), sizeof(glm::mat4));

        f.read(reinterpret_cast<char*>(&pathLen), sizeof(uint32_t));
        Ref<Texture2D> albedo;
        if (pathLen > 0)
        {
            std::string src(pathLen, '\0');
            f.read(src.data(), pathLen);
            if (src.size() > 6 && src.substr(0, 6) == "color:")
            {
                unsigned r, g, b, a;
                sscanf(src.c_str() + 6, "%02X%02X%02X%02X", &r, &g, &b, &a);
                uint8_t pixel[4] = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
                uint32_t packed; memcpy(&packed, pixel, 4);
                albedo = Texture2D::Create(1, 1, TextureFormat::RGBA8);
                albedo->SetData(&packed, 4);
                albedo->QueueUpload();
            }
            else albedo = Texture2D::Create(src);
        }
        Ref<Texture2D> mr     = ReadCachedTex(f);
        Ref<Texture2D> normal = ReadCachedTex(f);

        MeshDrawGroup dg;
        dg.Albedo                = albedo ? albedo : WhiteTexture();
        dg.MetalRoughness        = mr     ? mr     : DefaultMetalRoughness();
        dg.NormalMap             = normal ? normal : FlatNormalMap();
        dg.HasRealMetalRoughness = mr     != nullptr;
        dg.HasRealNormalMap      = normal != nullptr;
        dg.IndexByteOffset       = offset;
        dg.IndexCount            = count;
        dg.BaseVertex            = baseVertex;
        dg.NodeTransform         = nodeTransform;
        data.Groups.push_back(std::move(dg));
    }
    if (!f) return false;

    // Per-group AABBs in local space from the cached vertices.
    for (auto& dg : data.Groups)
    {
        dg.AABBMin = glm::vec3( FLT_MAX);
        dg.AABBMax = glm::vec3(-FLT_MAX);
        uint32_t idxStart = dg.IndexByteOffset / sizeof(uint32_t);
        for (uint32_t i = 0; i < dg.IndexCount; i++)
        {
            const glm::vec3& p = data.Vertices[dg.BaseVertex + data.Indices[idxStart + i]].Position;
            dg.AABBMin = glm::min(dg.AABBMin, p);
            dg.AABBMax = glm::max(dg.AABBMax, p);
        }
    }
    ComputeWorldAABB(data);

    glm::vec3 size = data.AABBMax - data.AABBMin;
    HMN_CORE_INFO("StaticMesh: cache loaded - {}v {}i {}groups", hdr.vertCount, hdr.idxCount, hdr.groupCount);
    HMN_CORE_INFO("StaticMesh: size  W:{} H:{} D:{}", FormatSize(size.x), FormatSize(size.y), FormatSize(size.z));
    return true;
}

void WriteCache(const std::string& binPath, const MeshData& data)
{
    std::ofstream f(binPath, std::ios::binary);
    if (!f) { HMN_CORE_WARN("StaticMesh: cannot write cache '{}'", binPath); return; }

    struct Header { uint32_t magic, version, vertCount, idxCount, groupCount; } hdr = {
        k_CacheMagic, k_CacheVersion,
        (uint32_t)data.Vertices.size(), (uint32_t)data.Indices.size(), (uint32_t)data.Groups.size()
    };
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(data.Vertices.data()), data.Vertices.size() * sizeof(StaticVertex));
    f.write(reinterpret_cast<const char*>(data.Indices.data()),  data.Indices.size()  * sizeof(uint32_t));

    auto writeStr = [&](const std::string& p)
    {
        uint32_t len = static_cast<uint32_t>(p.size());
        f.write(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
        if (len > 0) f.write(p.data(), len);
    };

    for (const auto& g : data.Groups)
    {
        f.write(reinterpret_cast<const char*>(&g.IndexByteOffset),              sizeof(uint32_t));
        f.write(reinterpret_cast<const char*>(&g.IndexCount),                   sizeof(uint32_t));
        f.write(reinterpret_cast<const char*>(&g.BaseVertex),                   sizeof(int32_t));
        f.write(reinterpret_cast<const char*>(glm::value_ptr(g.NodeTransform)), sizeof(glm::mat4));
        writeStr(g.AlbedoSrc);
        writeStr(g.MRSrc);
        writeStr(g.NormalSrc);
    }
    HMN_CORE_INFO("StaticMesh: wrote cache '{}'", binPath);
}

glm::mat4 ToGlm(const aiMatrix4x4& m)
{
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4);
}

std::expected<void, std::string> ImportAssimp(const std::string& path, MeshData& data)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, k_AssimpFlags);
    if (!scene || !scene->mRootNode)
        return std::unexpected(std::format("StaticMesh: failed to import '{}': {}", path, importer.GetErrorString()));

    std::string dir = path.substr(0, path.find_last_of("/\\"));

    uint32_t totalVerts = 0, totalIndices = 0;
    for (uint32_t i = 0; i < scene->mNumMeshes; i++)
    {
        totalVerts   += scene->mMeshes[i]->mNumVertices;
        totalIndices += scene->mMeshes[i]->mNumFaces * 3;
    }
    data.Vertices.reserve(totalVerts);
    data.Indices.reserve(totalIndices);

    struct RawSub { uint32_t idxOffset, idxCount; int32_t baseVertex; uint32_t matIdx; glm::mat4 nodeTransform{1.f}; };
    std::vector<RawSub> rawSubs;
    const aiVector3D kZero(0.f);

    // Walk the node tree; vertices arrive already in metres (aiProcess_GlobalScale).
    std::function<void(const aiNode*, const glm::mat4&)> traverse;
    traverse = [&](const aiNode* node, const glm::mat4& parentTransform)
    {
        glm::mat4 worldTransform = parentTransform * ToGlm(node->mTransformation);
        for (uint32_t mi = 0; mi < node->mNumMeshes; mi++)
        {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
            int32_t  baseVertex  = static_cast<int32_t>(data.Vertices.size());
            uint32_t baseIndex   = static_cast<uint32_t>(data.Indices.size());
            uint32_t faceIndices = 0;

            for (uint32_t v = 0; v < mesh->mNumVertices; v++)
            {
                const auto& p = mesh->mVertices[v];
                const auto& n = mesh->mNormals ? mesh->mNormals[v] : kZero;
                const auto& u = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][v] : kZero;

                glm::vec4 tangent(1, 0, 0, 1);
                if (mesh->mTangents && mesh->mBitangents)
                {
                    const auto& t = mesh->mTangents[v];
                    const auto& b = mesh->mBitangents[v];
                    glm::vec3 T(t.x, t.y, t.z), N(n.x, n.y, n.z), B(b.x, b.y, b.z);
                    float handedness = (glm::dot(glm::cross(N, T), B) < 0.f) ? -1.f : 1.f;
                    tangent = glm::vec4(T, handedness);
                }
                data.Vertices.push_back({ { p.x, p.y, p.z }, { n.x, n.y, n.z }, { u.x, u.y }, tangent });
            }

            for (uint32_t fc = 0; fc < mesh->mNumFaces; fc++)
            {
                const aiFace& face = mesh->mFaces[fc];
                if (face.mNumIndices != 3) continue;
                data.Indices.push_back(face.mIndices[0]);
                data.Indices.push_back(face.mIndices[1]);
                data.Indices.push_back(face.mIndices[2]);
                faceIndices += 3;
            }
            rawSubs.push_back({ baseIndex, faceIndices, baseVertex, mesh->mMaterialIndex, worldTransform });
        }
        for (uint32_t ci = 0; ci < node->mNumChildren; ci++)
            traverse(node->mChildren[ci], worldTransform);
    };
    traverse(scene->mRootNode, glm::mat4(1.f));

    // --- materials ---
    std::vector<Ref<Texture2D>> matAlbedo(scene->mNumMaterials);
    std::vector<Ref<Texture2D>> matMR(scene->mNumMaterials);
    std::vector<Ref<Texture2D>> matNormal(scene->mNumMaterials);
    std::vector<std::string>    matAlbedoSrc(scene->mNumMaterials);
    std::vector<std::string>    matMRSrc(scene->mNumMaterials);
    std::vector<std::string>    matNormalSrc(scene->mNumMaterials);

    // Albedo lives under BASE_COLOR (glTF) or DIFFUSE (legacy/FBX).
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
        const aiMaterial* mat = scene->mMaterials[i];

        aiString aiTexPath;
        if (findBaseColor(mat, aiTexPath))
        {
            std::string raw = aiTexPath.data;
            if (!raw.empty() && raw[0] == '*')
            {
                matAlbedoSrc[i] = raw;
                matAlbedo[i]    = LoadEmbeddedTexture(scene, raw);
            }
            else
            {
                std::string resolved = ResolveTexturePath(raw, dir);
                if (!resolved.empty())
                {
                    matAlbedoSrc[i] = resolved;
                    matAlbedo[i]    = Texture2D::Create(resolved);
                }
            }
        }

        if (!matAlbedo[i])
        {
            aiColor4D diffuse(0.7f, 0.7f, 0.7f, 1.f);
            aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &diffuse);
            if (diffuse.r == 0 && diffuse.g == 0 && diffuse.b == 0)
                aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse);

            char buf[20];
            snprintf(buf, sizeof(buf), "color:%02X%02X%02X%02X",
                (uint8_t)(diffuse.r * 255), (uint8_t)(diffuse.g * 255),
                (uint8_t)(diffuse.b * 255), (uint8_t)(diffuse.a * 255));
            matAlbedoSrc[i] = buf;
            matAlbedo[i]    = MakeColorTexture(diffuse);
        }

        aiString nmPath;
        if (mat->GetTextureCount(aiTextureType_NORMALS) > 0 &&
            mat->GetTexture(aiTextureType_NORMALS, 0, &nmPath) == AI_SUCCESS)
        {
            std::string raw = nmPath.data;
            if (!raw.empty() && raw[0] == '*') { matNormal[i] = LoadEmbeddedTexture(scene, raw); matNormalSrc[i] = raw; }
            else
            {
                std::string resolved = ResolveTexturePath(raw, dir);
                if (!resolved.empty()) { matNormal[i] = Texture2D::Create(resolved); matNormalSrc[i] = resolved; }
            }
        }

        aiString mrPath;
        if (mat->GetTextureCount(aiTextureType_METALNESS) > 0 &&
            mat->GetTexture(aiTextureType_METALNESS, 0, &mrPath) == AI_SUCCESS)
        {
            std::string raw = mrPath.data;
            if (!raw.empty() && raw[0] == '*') { matMR[i] = LoadEmbeddedTexture(scene, raw); matMRSrc[i] = raw; }
            else
            {
                std::string resolved = ResolveTexturePath(raw, dir);
                if (!resolved.empty()) { matMR[i] = Texture2D::Create(resolved); matMRSrc[i] = resolved; }
            }
        }
    }

    // Sort submeshes by material to minimise texture rebinds, then build draw groups.
    std::sort(rawSubs.begin(), rawSubs.end(), [](const RawSub& a, const RawSub& b) { return a.matIdx < b.matIdx; });

    data.Groups.clear();
    data.Groups.reserve(rawSubs.size());
    for (const auto& sub : rawSubs)
    {
        MeshDrawGroup dg;
        dg.Albedo                = (sub.matIdx < matAlbedo.size() && matAlbedo[sub.matIdx]) ? matAlbedo[sub.matIdx] : WhiteTexture();
        dg.MetalRoughness        = (sub.matIdx < matMR.size()     && matMR[sub.matIdx])     ? matMR[sub.matIdx]     : DefaultMetalRoughness();
        dg.NormalMap             = (sub.matIdx < matNormal.size() && matNormal[sub.matIdx]) ? matNormal[sub.matIdx] : FlatNormalMap();
        dg.AlbedoSrc             = sub.matIdx < matAlbedoSrc.size() ? matAlbedoSrc[sub.matIdx] : "";
        dg.MRSrc                 = sub.matIdx < matMRSrc.size()     ? matMRSrc[sub.matIdx]     : "";
        dg.NormalSrc             = sub.matIdx < matNormalSrc.size() ? matNormalSrc[sub.matIdx] : "";
        dg.HasRealMetalRoughness = !dg.MRSrc.empty();
        dg.HasRealNormalMap      = !dg.NormalSrc.empty();
        dg.IndexByteOffset       = sub.idxOffset * (uint32_t)sizeof(uint32_t);
        dg.IndexCount            = sub.idxCount;
        dg.BaseVertex            = sub.baseVertex;
        dg.NodeTransform         = sub.nodeTransform;
        data.Groups.push_back(std::move(dg));
    }

    OptimizeGeometry(data);
    ComputeWorldAABB(data);

    glm::vec3 size = data.AABBMax - data.AABBMin;
    HMN_CORE_INFO("StaticMesh: '{}' - {}v {}i {}groups", path, data.Vertices.size(), data.Indices.size(), data.Groups.size());
    HMN_CORE_INFO("StaticMesh: size  W:{} H:{} D:{}", FormatSize(size.x), FormatSize(size.y), FormatSize(size.z));
    return {};
}

} // namespace

std::expected<MeshData, std::string> ImportStaticMesh(const std::string& path)
{
    // glTF/GLB embed textures that can't be re-resolved from a path cache, so skip caching them.
    const bool canCache = !IsGltf(path);

    if (canCache)
    {
        std::string binPath = path + ".bin";
        if (std::filesystem::exists(binPath))
        {
            HMN_CORE_INFO("StaticMesh: loading from cache '{}'", binPath);
            MeshData cached;
            if (ReadCache(binPath, cached))
                return cached;
            HMN_CORE_WARN("StaticMesh: cache corrupt, re-importing '{}'", path);
        }
    }

    MeshData data;
    if (auto res = ImportAssimp(path, data); !res)
        return std::unexpected(res.error());

    if (canCache)
        WriteCache(path + ".bin", data);

    return data;
}

}
