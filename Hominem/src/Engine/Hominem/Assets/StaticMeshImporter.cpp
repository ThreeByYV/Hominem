#include "hmnpch.h"
#include "StaticMeshImporter.h"
#include "MaterialTextures.h"

#include "Hominem/Core/Log.h"
#include "Hominem/Utils/FileUtils.h"
#include "Hominem/Utils/AssimpGlm.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/ProgressHandler.hpp>
#include <meshoptimizer.h>

#include <fstream>
#include <filesystem>
#include <functional>

namespace Hominem {

namespace {

constexpr uint32_t k_CacheMagic   = 0x48534D53u; // "SMSH"
constexpr uint32_t k_CacheVersion = 9u;          // v9: full materials

constexpr unsigned int k_AssimpFlags =
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
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

    data.Materials.clear();
    data.Materials.reserve(hdr.matCount);
    for (uint32_t i = 0; i < hdr.matCount; i++)
        data.Materials.push_back(Realize(ReadMaterialSource(f)));

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
        dg.MaterialIndex   = matIdx;
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
    const std::vector<MaterialSource>& materials)
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
        Write(f, m);

    for (size_t i = 0; i < data.Groups.size(); i++)
    {
        const auto& g = data.Groups[i];
        FileUtils::WriteValue(f, g.IndexByteOffset);
        FileUtils::WriteValue(f, g.IndexCount);
        FileUtils::WriteValue(f, g.BaseVertex);
        FileUtils::WriteValue(f, g.NodeTransform);
        FileUtils::WriteValue(f, g.MaterialIndex);
    }
    HMN_CORE_INFO("StaticMesh: wrote cache '{}'", binPath);
}

// assimp import

class ImportProgressHandler : public Assimp::ProgressHandler
{
public:
    ImportProgressHandler(const std::string& path) : m_Path(path) {}
    bool Update(float percentage) override
    {
        int pct = (int)(percentage * 100.f);
        if (pct >= m_NextLog)
        {
            HMN_CORE_INFO("StaticMesh: importing '{}' ... {}%", m_Path, pct);
            m_NextLog = ((pct / 25) + 1) * 25;
        }
        return true;
    }
private:
    std::string m_Path;
    int m_NextLog = 25;
};

std::expected<void, std::string> ImportAssimp(const std::string& path, MeshData& data,
    std::vector<MaterialSource>& outMaterials)
{
    Assimp::Importer importer;
    importer.SetProgressHandler(new ImportProgressHandler(path));
    HMN_CORE_INFO("StaticMesh: importing '{}' via assimp (building cache)...", path);
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

    outMaterials.resize(scene->mNumMaterials);
    data.Materials.resize(scene->mNumMaterials);
    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
    {
        outMaterials[i]   = DescribeMaterial(scene, scene->mMaterials[i], dir);
        data.Materials[i] = Realize(outMaterials[i]);
    }

    // Sort submeshes by material to minimise texture rebinds, then build draw groups.
    std::sort(rawSubs.begin(), rawSubs.end(), [](const RawSub& a, const RawSub& b) { return a.matIdx < b.matIdx; });

    data.Groups.clear();
    data.Groups.reserve(rawSubs.size());
    for (const auto& sub : rawSubs)
    {
        MeshDrawGroup dg;
        dg.MaterialIndex   = sub.matIdx;
        dg.IndexByteOffset = sub.idxOffset * (uint32_t)sizeof(uint32_t);
        dg.IndexCount      = sub.idxCount;
        dg.BaseVertex      = sub.baseVertex;
        dg.NodeTransform   = sub.nodeTransform;
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
    std::vector<MaterialSource> materials;
    if (auto res = ImportAssimp(path, data, materials); !res)
        return std::unexpected(res.error());

    WriteCache(binPath, data, materials);
    return data;
}

}
