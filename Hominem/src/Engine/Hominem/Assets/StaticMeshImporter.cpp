#include "hmnpch.h"
#include "StaticMeshImporter.h"
#include "MaterialTextures.h"

#include "Hominem/Core/Log.h"
#include "Hominem/Utils/FileUtils.h"
#include "Hominem/Utils/AssimpGlm.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <meshoptimizer.h>

#include <fstream>
#include <filesystem>
#include <functional>

namespace Hominem {

namespace {

constexpr uint32_t k_CacheMagic   = 0x48534D53u; // "SMSH"
constexpr uint32_t k_CacheVersion = 8u;          // v8: per-material textures, embedded bytes baked in (glTF cacheable)

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

// A way to rebuild a material texture without the source model: an external
// path, a solid colour, or embedded image bytes (compressed blob or raw RGBA).
struct CachedTexture
{
    enum class Kind : uint32_t { None = 0, Path, Color, EmbeddedCompressed, EmbeddedRaw };
    Kind kind = Kind::None;
    std::string          path;             // Path
    uint32_t             color = 0;        // Color (packed R,G,B,A bytes)
    uint32_t             width = 0, height = 0; // EmbeddedRaw
    std::vector<uint8_t> bytes;            // EmbeddedCompressed blob / EmbeddedRaw pixels

    bool Present() const { return kind != Kind::None; }
};

struct MaterialTexSet { CachedTexture Albedo, MR, Normal; };

Ref<Texture2D> Realize(const CachedTexture& t)
{
    switch (t.kind)
    {
        case CachedTexture::Kind::Path:
            return Texture2D::Create(t.path);
        case CachedTexture::Kind::Color:
        {
            auto tex = Texture2D::Create(1, 1, TextureFormat::RGBA8);
            tex->SetData(&t.color, 4);
            tex->QueueUpload();
            return tex;
        }
        case CachedTexture::Kind::EmbeddedCompressed:
            return Texture2D::CreateFromMemory(t.bytes.data(), static_cast<uint32_t>(t.bytes.size()));
        case CachedTexture::Kind::EmbeddedRaw:
        {
            auto tex = Texture2D::Create(t.width, t.height, TextureFormat::RGBA8);
            tex->SetData(t.bytes.data(), static_cast<uint32_t>(t.bytes.size()));
            tex->QueueUpload();
            return tex;
        }
        case CachedTexture::Kind::None:
        default:
            return nullptr;
    }
}

// Describe a raw Assimp texture reference: "*N" embedded (bytes captured) or a file path.
CachedTexture DescribeRaw(const aiScene* scene, const std::string& raw, const std::string& dir)
{
    CachedTexture t;
    if (raw.empty()) return t;

    if (raw[0] == '*')
    {
        const aiTexture* emb = scene->GetEmbeddedTexture(raw.c_str());
        if (!emb) return t;

        if (emb->mHeight == 0)
        {
            // Compressed blob (PNG/JPG) - mWidth is the byte count.
            t.kind = CachedTexture::Kind::EmbeddedCompressed;
            const uint8_t* p = reinterpret_cast<const uint8_t*>(emb->pcData);
            t.bytes.assign(p, p + emb->mWidth);
        }
        else
        {
            // Raw ARGB8888 -> RGBA.
            t.kind   = CachedTexture::Kind::EmbeddedRaw;
            t.width  = emb->mWidth;
            t.height = emb->mHeight;
            uint32_t n = emb->mWidth * emb->mHeight;
            t.bytes.resize(n * 4);
            for (uint32_t i = 0; i < n; i++)
            {
                t.bytes[i * 4 + 0] = emb->pcData[i].r;
                t.bytes[i * 4 + 1] = emb->pcData[i].g;
                t.bytes[i * 4 + 2] = emb->pcData[i].b;
                t.bytes[i * 4 + 3] = emb->pcData[i].a;
            }
        }
        return t;
    }

    std::string resolved = ResolveTexturePath(raw, dir);
    if (resolved.empty()) return t;
    t.kind = CachedTexture::Kind::Path;
    t.path = resolved;
    return t;
}

CachedTexture DescribeTexture(const aiScene* scene, const aiMaterial* mat, aiTextureType type, const std::string& dir)
{
    if (mat->GetTextureCount(type) == 0) return {};
    aiString texPath;
    if (mat->GetTexture(type, 0, &texPath) != AI_SUCCESS) return {};
    return DescribeRaw(scene, texPath.C_Str(), dir);
}

// Albedo: BASE_COLOR (glTF) or DIFFUSE (legacy/FBX) texture, else the material's solid colour.
CachedTexture DescribeAlbedo(const aiScene* scene, const aiMaterial* mat, const std::string& dir)
{
    aiString p;
    bool found = (mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0 && mat->GetTexture(aiTextureType_BASE_COLOR, 0, &p) == AI_SUCCESS)
              || (mat->GetTextureCount(aiTextureType_DIFFUSE)    > 0 && mat->GetTexture(aiTextureType_DIFFUSE,    0, &p) == AI_SUCCESS);

    if (found)
    {
        CachedTexture t = DescribeRaw(scene, p.C_Str(), dir);
        if (t.Present()) return t;
    }

    aiColor4D diffuse(0.7f, 0.7f, 0.7f, 1.f);
    aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &diffuse);
    if (diffuse.r == 0 && diffuse.g == 0 && diffuse.b == 0)
        aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse);

    uint8_t px[4] = {
        (uint8_t)(glm::clamp(diffuse.r, 0.f, 1.f) * 255),
        (uint8_t)(glm::clamp(diffuse.g, 0.f, 1.f) * 255),
        (uint8_t)(glm::clamp(diffuse.b, 0.f, 1.f) * 255),
        (uint8_t)(glm::clamp(diffuse.a, 0.f, 1.f) * 255)
    };
    CachedTexture t;
    t.kind = CachedTexture::Kind::Color;
    memcpy(&t.color, px, 4);
    return t;
}

void WriteCachedTexture(std::ostream& os, const CachedTexture& t)
{
    FileUtils::WriteValue(os, static_cast<uint32_t>(t.kind));
    switch (t.kind)
    {
        case CachedTexture::Kind::Path:  FileUtils::WriteString(os, t.path); break;
        case CachedTexture::Kind::Color: FileUtils::WriteValue(os, t.color); break;
        case CachedTexture::Kind::EmbeddedCompressed:
            FileUtils::WriteValue(os, static_cast<uint32_t>(t.bytes.size()));
            FileUtils::WriteArray(os, t.bytes);
            break;
        case CachedTexture::Kind::EmbeddedRaw:
            FileUtils::WriteValue(os, t.width);
            FileUtils::WriteValue(os, t.height);
            FileUtils::WriteValue(os, static_cast<uint32_t>(t.bytes.size()));
            FileUtils::WriteArray(os, t.bytes);
            break;
        case CachedTexture::Kind::None:
        default: break;
    }
}

CachedTexture ReadCachedTexture(std::istream& is)
{
    CachedTexture t;
    uint32_t k = 0;
    FileUtils::ReadValue(is, k);
    t.kind = static_cast<CachedTexture::Kind>(k);
    switch (t.kind)
    {
        case CachedTexture::Kind::Path:  t.path = FileUtils::ReadString(is); break;
        case CachedTexture::Kind::Color: FileUtils::ReadValue(is, t.color);  break;
        case CachedTexture::Kind::EmbeddedCompressed:
        {
            uint32_t n = 0; FileUtils::ReadValue(is, n);
            FileUtils::ReadArray(is, t.bytes, n);
            break;
        }
        case CachedTexture::Kind::EmbeddedRaw:
        {
            FileUtils::ReadValue(is, t.width);
            FileUtils::ReadValue(is, t.height);
            uint32_t n = 0; FileUtils::ReadValue(is, n);
            FileUtils::ReadArray(is, t.bytes, n);
            break;
        }
        case CachedTexture::Kind::None:
        default: break;
    }
    return t;
}

// geometry helpers

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

// Build runtime draw groups from per-material textures, with the engine defaults as fallback.
void AssignGroupTextures(MeshDrawGroup& dg, uint32_t matIdx,
    const std::vector<MaterialTexSet>& materials,
    const std::vector<Ref<Texture2D>>& albedo,
    const std::vector<Ref<Texture2D>>& mr,
    const std::vector<Ref<Texture2D>>& normal)
{
    dg.Albedo         = (matIdx < albedo.size() && albedo[matIdx]) ? albedo[matIdx] : WhiteTexture();
    dg.MetalRoughness = (matIdx < mr.size()     && mr[matIdx])     ? mr[matIdx]     : DefaultMetalRoughness();
    dg.NormalMap      = (matIdx < normal.size() && normal[matIdx]) ? normal[matIdx] : FlatNormalMap();
    dg.HasRealMetalRoughness = matIdx < materials.size() && materials[matIdx].MR.Present();
    dg.HasRealNormalMap      = matIdx < materials.size() && materials[matIdx].Normal.Present();
}

// --- cache I/O --------------------------------------------------------------

struct CacheHeader { uint32_t magic, version, vertCount, idxCount, matCount, groupCount; };

bool ReadCache(const std::string& binPath, MeshData& data)
{
    std::ifstream f(binPath, std::ios::binary);
    if (!f) return false;

    CacheHeader hdr;
    if (!FileUtils::ReadValue(f, hdr) || hdr.magic != k_CacheMagic || hdr.version != k_CacheVersion)
        return false;

    FileUtils::ReadArray(f, data.Vertices, hdr.vertCount);
    FileUtils::ReadArray(f, data.Indices,  hdr.idxCount);

    // Materials: one realized texture per material, shared across groups.
    std::vector<MaterialTexSet>  materials(hdr.matCount);
    std::vector<Ref<Texture2D>>  matAlbedo(hdr.matCount), matMR(hdr.matCount), matNormal(hdr.matCount);
    for (uint32_t i = 0; i < hdr.matCount; i++)
    {
        materials[i].Albedo = ReadCachedTexture(f);
        materials[i].MR     = ReadCachedTexture(f);
        materials[i].Normal = ReadCachedTexture(f);
        matAlbedo[i] = Realize(materials[i].Albedo);
        matMR[i]     = Realize(materials[i].MR);
        matNormal[i] = Realize(materials[i].Normal);
    }

    data.Groups.clear();
    data.Groups.reserve(hdr.groupCount);
    for (uint32_t i = 0; i < hdr.groupCount; i++)
    {
        uint32_t  offset, count, matIdx;
        int32_t   baseVertex;
        glm::mat4 nodeTransform(1.f);
        FileUtils::ReadValue(f, offset);
        FileUtils::ReadValue(f, count);
        FileUtils::ReadValue(f, baseVertex);
        FileUtils::ReadValue(f, nodeTransform);
        FileUtils::ReadValue(f, matIdx);

        MeshDrawGroup dg;
        AssignGroupTextures(dg, matIdx, materials, matAlbedo, matMR, matNormal);
        dg.IndexByteOffset = offset;
        dg.IndexCount      = count;
        dg.BaseVertex      = baseVertex;
        dg.NodeTransform   = nodeTransform;
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

void WriteCache(const std::string& binPath, const MeshData& data,
    const std::vector<MaterialTexSet>& materials,
    const std::vector<uint32_t>& groupMatIndex)
{
    std::ofstream f(binPath, std::ios::binary);
    if (!f) { HMN_CORE_WARN("StaticMesh: cannot write cache '{}'", binPath); return; }

    CacheHeader hdr = {
        k_CacheMagic, k_CacheVersion,
        (uint32_t)data.Vertices.size(), (uint32_t)data.Indices.size(),
        (uint32_t)materials.size(), (uint32_t)data.Groups.size()
    };
    FileUtils::WriteValue(f, hdr);
    FileUtils::WriteArray(f, data.Vertices);
    FileUtils::WriteArray(f, data.Indices);

    for (const auto& m : materials)
    {
        WriteCachedTexture(f, m.Albedo);
        WriteCachedTexture(f, m.MR);
        WriteCachedTexture(f, m.Normal);
    }

    for (size_t i = 0; i < data.Groups.size(); i++)
    {
        const auto& g = data.Groups[i];
        FileUtils::WriteValue(f, g.IndexByteOffset);
        FileUtils::WriteValue(f, g.IndexCount);
        FileUtils::WriteValue(f, g.BaseVertex);
        FileUtils::WriteValue(f, g.NodeTransform);
        FileUtils::WriteValue(f, groupMatIndex[i]);
    }
    HMN_CORE_INFO("StaticMesh: wrote cache '{}'", binPath);
}

// assimp import

std::expected<void, std::string> ImportAssimp(const std::string& path, MeshData& data,
    std::vector<MaterialTexSet>& outMaterials, std::vector<uint32_t>& outGroupMatIndex)
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
        glm::mat4 worldTransform = parentTransform * AiToGlm(node->mTransformation);
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

    // Describe + realize one texture set per material (shared across groups).
    outMaterials.resize(scene->mNumMaterials);
    std::vector<Ref<Texture2D>> matAlbedo(scene->mNumMaterials);
    std::vector<Ref<Texture2D>> matMR(scene->mNumMaterials);
    std::vector<Ref<Texture2D>> matNormal(scene->mNumMaterials);
    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
    {
        const aiMaterial* mat = scene->mMaterials[i];
        outMaterials[i].Albedo = DescribeAlbedo(scene, mat, dir);
        outMaterials[i].MR     = DescribeTexture(scene, mat, aiTextureType_METALNESS, dir);
        outMaterials[i].Normal = DescribeTexture(scene, mat, aiTextureType_NORMALS,   dir);
        matAlbedo[i] = Realize(outMaterials[i].Albedo);
        matMR[i]     = Realize(outMaterials[i].MR);
        matNormal[i] = Realize(outMaterials[i].Normal);
    }

    // Sort submeshes by material to minimise texture rebinds, then build draw groups.
    std::sort(rawSubs.begin(), rawSubs.end(), [](const RawSub& a, const RawSub& b) { return a.matIdx < b.matIdx; });

    data.Groups.clear();
    data.Groups.reserve(rawSubs.size());
    outGroupMatIndex.clear();
    outGroupMatIndex.reserve(rawSubs.size());
    for (const auto& sub : rawSubs)
    {
        MeshDrawGroup dg;
        AssignGroupTextures(dg, sub.matIdx, outMaterials, matAlbedo, matMR, matNormal);
        dg.IndexByteOffset = sub.idxOffset * (uint32_t)sizeof(uint32_t);
        dg.IndexCount      = sub.idxCount;
        dg.BaseVertex      = sub.baseVertex;
        dg.NodeTransform   = sub.nodeTransform;
        data.Groups.push_back(std::move(dg));
        outGroupMatIndex.push_back(sub.matIdx);
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
    const std::string binPath = path + ".bin";

    if (std::filesystem::exists(binPath))
    {
        HMN_CORE_INFO("StaticMesh: loading from cache '{}'", binPath);
        MeshData cached;
        if (ReadCache(binPath, cached))
            return cached;
        HMN_CORE_WARN("StaticMesh: cache corrupt, re-importing '{}'", path);
    }

    MeshData data;
    std::vector<MaterialTexSet> materials;
    std::vector<uint32_t>       groupMatIndex;
    if (auto res = ImportAssimp(path, data, materials, groupMatIndex); !res)
        return std::unexpected(res.error());

    WriteCache(binPath, data, materials, groupMatIndex);
    return data;
}

}
