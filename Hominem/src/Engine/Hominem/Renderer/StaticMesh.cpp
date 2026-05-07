#include "hmnpch.h"
#include "StaticMesh.h"

#include <glad/glad.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Hominem
{
    static constexpr uint32_t k_CacheMagic   = 0x48534D53u; // "SMSH"
    static constexpr uint32_t k_CacheVersion = 2u;          // v2: colour fallback paths

    constexpr unsigned int k_AssimpFlags =
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs |
            aiProcess_JoinIdenticalVertices |
            aiProcess_OptimizeMeshes |
            aiProcess_PreTransformVertices; // bakes node hierarchy into vertex positions

    static Ref<Texture2D> s_WhiteTexture;

    static Ref<Texture2D> GetWhiteTexture()
    {
        if (!s_WhiteTexture)
        {
            s_WhiteTexture = Texture2D::Create(1, 1, TextureFormat::RGBA8);
            uint32_t white = 0xFFFFFFFFu;
            s_WhiteTexture->SetData(&white, sizeof(white));
        }
        return s_WhiteTexture;
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

    bool StaticMesh::LoadFromFile(const std::string &path)
    {
        std::string binPath = path + ".bin";

        if (std::filesystem::exists(binPath))
        {
            HMN_CORE_INFO("StaticMesh: loading from cache '{}'", binPath);
            if (LoadBinary(binPath))
                return true;
            HMN_CORE_WARN("StaticMesh: cache corrupt, re-importing '{}'", path);
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

            Ref<Texture2D> tex;
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
                    tex = Texture2D::Create(1, 1, TextureFormat::RGBA8);
                    tex->SetData(&packed, 4);
                }
                else
                {
                    tex = Texture2D::Create(texPath);
                }
            }

            m_DrawGroups.push_back({tex ? tex : GetWhiteTexture(), offset, count, baseVertex});
        }

        if (!f) return false;

        m_AABBMin = glm::vec3(FLT_MAX);
        m_AABBMax = glm::vec3(-FLT_MAX);
        for (const auto& v : verts)
        {
            m_AABBMin = glm::min(m_AABBMin, v.Position);
            m_AABBMax = glm::max(m_AABBMax, v.Position);
        }

        Upload(verts, indices);
        HMN_CORE_INFO("StaticMesh: cache loaded — {}v {}i {}groups",
                      hdr.vertCount, hdr.idxCount, hdr.groupCount);
        return true;
    }

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
                verts.push_back({{p.x, p.y, p.z}, {n.x, n.y, n.z}, {u.x, u.y}});
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
        std::vector<Ref<Texture2D> > matTextures(scene->mNumMaterials);
        std::vector<std::string> matPaths(scene->mNumMaterials);

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
            return tex;
        };

        for (uint32_t i = 0; i < scene->mNumMaterials; i++)
        {
            const aiMaterial *mat = scene->mMaterials[i];

            if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            {
                aiString aiTexPath;
                if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS)
                {
                    std::string resolved = resolveTexture(std::string(aiTexPath.data));
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
            }

            // No texture — use material diffuse colour as a 1×1 fallback
            aiColor4D diffuse(0.7f, 0.7f, 0.7f, 1.f);
            aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse);

            char buf[20];
            snprintf(buf, sizeof(buf), "color:%02X%02X%02X%02X",
                (uint8_t)(diffuse.r * 255), (uint8_t)(diffuse.g * 255),
                (uint8_t)(diffuse.b * 255), (uint8_t)(diffuse.a * 255));

            matPaths[i]    = buf;
            matTextures[i] = makeColorTex(diffuse);
            HMN_CORE_INFO("StaticMesh: material [{}] color fallback {}", i, buf);
        }

        // --- sort by material to minimise texture rebinds ---
        std::sort(rawSubs.begin(), rawSubs.end(),
                  [](const RawSub &a, const RawSub &b) { return a.matIdx < b.matIdx; });

        // --- build draw groups ---
        m_DrawGroups.clear();
        std::vector<std::string> groupTexPaths;
        m_DrawGroups.reserve(rawSubs.size());
        groupTexPaths.reserve(rawSubs.size());

        for (const auto &sub: rawSubs)
        {
            auto tex = (sub.matIdx < matTextures.size() && matTextures[sub.matIdx])
                           ? matTextures[sub.matIdx]
                           : GetWhiteTexture();

            const std::string &texPath = sub.matIdx < matPaths.size() ? matPaths[sub.matIdx] : "";

            m_DrawGroups.push_back({tex, sub.idxOffset * (uint32_t) sizeof(uint32_t), sub.idxCount, sub.baseVertex});
            groupTexPaths.push_back(texPath);
        }

        m_AABBMin = glm::vec3(FLT_MAX);
        m_AABBMax = glm::vec3(-FLT_MAX);
        for (const auto &v: verts)
        {
            m_AABBMin = glm::min(m_AABBMin, v.Position);
            m_AABBMax = glm::max(m_AABBMax, v.Position);
        }
        HMN_CORE_INFO("StaticMesh: '{}' — {}v {}i {}groups", path, verts.size(), indices.size(), rawSubs.size());
        HMN_CORE_INFO("StaticMesh: AABB min({:.1f},{:.1f},{:.1f}) max({:.1f},{:.1f},{:.1f})",
                      m_AABBMin.x, m_AABBMin.y, m_AABBMin.z, m_AABBMax.x, m_AABBMax.y, m_AABBMax.z);

        WriteBinary(path + ".bin", verts, indices, m_DrawGroups, groupTexPaths);
        Upload(verts, indices);
        return true;
    }

    void StaticMesh::WriteBinary(const std::string &binPath,
                                 const std::vector<StaticVertex> &verts,
                                 const std::vector<uint32_t> &indices,
                                 const std::vector<DrawGroup> &groups,
                                 const std::vector<std::string> &groupTexPaths)
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

        for (size_t i = 0; i < groups.size(); i++)
        {
            const auto &g = groups[i];
            const std::string &tex = i < groupTexPaths.size() ? groupTexPaths[i] : "";
            uint32_t pathLen = static_cast<uint32_t>(tex.size());

            f.write(reinterpret_cast<const char *>(&g.IndexByteOffset), sizeof(uint32_t));
            f.write(reinterpret_cast<const char *>(&g.IndexCount), sizeof(uint32_t));
            f.write(reinterpret_cast<const char *>(&g.BaseVertex), sizeof(int32_t));
            f.write(reinterpret_cast<const char *>(&pathLen), sizeof(uint32_t));
            if (pathLen > 0)
                f.write(tex.data(), pathLen);
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
        shader->SetInt("u_Texture", 0);

        glBindVertexArray(m_VAO);

        for (const auto &group: m_DrawGroups)
        {
            if (group.Texture)
                group.Texture->Bind(0);

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
