#include "hmnpch.h"
#include "MaterialTextures.h"

#include "Hominem/Core/Log.h"

#include <assimp/scene.h>
#include <glm/common.hpp>

#include <algorithm>
#include <filesystem>

namespace Hominem {

std::string ResolveTexturePath(const std::string& rawPath, const std::string& baseDir)
{
    std::string norm = rawPath;
    std::replace(norm.begin(), norm.end(), '\\', '/');

    std::string filename = norm.substr(norm.find_last_of('/') + 1);

    // Build extension alternatives (.jpg <-> .jpeg).
    auto withAltExt = [](const std::string& p) -> std::string
    {
        auto dot = p.rfind('.');
        if (dot == std::string::npos) return "";
        std::string ext = p.substr(dot + 1);
        if (ext == "jpg")  return p.substr(0, dot + 1) + "jpeg";
        if (ext == "jpeg") return p.substr(0, dot + 1) + "jpg";
        return "";
    };

    const std::vector candidates = {
        norm,                               // absolute or already-correct relative
        baseDir + "/" + norm,                  // relative to the model's directory
        baseDir + "/" + filename,              // filename only, next to the model
        baseDir + "/textures/" + filename,     // textures/ sibling of the model
        baseDir + "/../textures/" + filename,  // textures/ one level up
    };

    for (const auto& c : candidates)
    {
        if (std::filesystem::exists(c))
            return std::filesystem::path(c).lexically_normal().string();
        std::string alt = withAltExt(c);
        if (!alt.empty() && std::filesystem::exists(alt))
            return std::filesystem::path(alt).lexically_normal().string();
    }

    HMN_CORE_WARN("MaterialTextures: could not resolve texture '{}' - tried:", rawPath);
    for (const auto& c : candidates)
        HMN_CORE_WARN("  {}", c);
    return "";
}

Ref<Texture2D> LoadEmbeddedTexture(const aiScene* scene, const std::string& rawPath)
{
    if (!scene || rawPath.empty() || rawPath[0] != '*') return nullptr;

    const aiTexture* emb = scene->GetEmbeddedTexture(rawPath.c_str());
    if (!emb) return nullptr;

    if (emb->mHeight == 0)
    {
        // Compressed blob (PNG/JPG) - mWidth holds the byte count. Auto-uploads.
        return Texture2D::CreateFromMemory(
            reinterpret_cast<const uint8_t*>(emb->pcData), emb->mWidth);
    }

    // Raw ARGB8888 - convert to RGBA and upload.
    const uint32_t numPixels = emb->mWidth * emb->mHeight;
    std::vector<uint8_t> rgba(numPixels * 4);
    for (uint32_t p = 0; p < numPixels; p++)
    {
        rgba[p * 4 + 0] = emb->pcData[p].r;
        rgba[p * 4 + 1] = emb->pcData[p].g;
        rgba[p * 4 + 2] = emb->pcData[p].b;
        rgba[p * 4 + 3] = emb->pcData[p].a;
    }
    auto tex = Texture2D::Create(emb->mWidth, emb->mHeight, TextureFormat::RGBA8);
    tex->SetData(rgba.data(), (uint32_t)rgba.size());
    tex->QueueUpload();
    return tex;
}

Ref<Texture2D> MakeColorTexture(const aiColor4D& color)
{
    uint8_t pixel[4] = {
        (uint8_t)(glm::clamp(color.r, 0.f, 1.f) * 255),
        (uint8_t)(glm::clamp(color.g, 0.f, 1.f) * 255),
        (uint8_t)(glm::clamp(color.b, 0.f, 1.f) * 255),
        (uint8_t)(glm::clamp(color.a, 0.f, 1.f) * 255)
    };
    uint32_t packed;
    memcpy(&packed, pixel, 4);
    auto tex = Texture2D::Create(1, 1, TextureFormat::RGBA8);
    tex->SetData(&packed, 4);
    tex->QueueUpload();
    return tex;
}

Ref<Texture2D> LoadMaterialTexture(const aiScene* scene, const aiMaterial* mat,
                                   aiTextureType type, const std::string& baseDir)
{
    if (!mat || mat->GetTextureCount(type) == 0) return nullptr;

    aiString texPath;
    if (mat->GetTexture(type, 0, &texPath) != AI_SUCCESS) return nullptr;

    std::string raw = texPath.C_Str();
    if (!raw.empty() && raw[0] == '*')
        return LoadEmbeddedTexture(scene, raw);

    std::string resolved = ResolveTexturePath(raw, baseDir);
    if (resolved.empty()) return nullptr;
    return Texture2D::Create(resolved);
}

}
