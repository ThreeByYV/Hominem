#include "hmnpch.h"
#include "MaterialTextures.h"

#include "Hominem/Core/Log.h"
#include "Hominem/Utils/FileUtils.h"

#include <assimp/scene.h>
#include <glm/common.hpp>

#include <algorithm>
#include <filesystem>
#include <istream>
#include <ostream>

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

namespace {
    Ref<Texture2D> MakeSolid(uint32_t rgba)
    {
        auto tex = Texture2D::Create(1, 1, TextureFormat::RGBA8);
        tex->SetData(&rgba, sizeof(rgba));
        tex->QueueUpload();
        return tex;
    }
}

Ref<Texture2D> WhiteTexture()
{
    static Ref<Texture2D> s = MakeSolid(0xFFFFFFFFu);
    return s;
}

Ref<Texture2D> FlatNormalMap()
{
    // (128,128,255) decodes to normal (0,0,1)
    static Ref<Texture2D> s = MakeSolid(0xFFFF8080u);
    return s;
}

namespace {

TextureSource DescribeRaw(const aiScene* scene, const std::string& raw, const std::string& dir)
{
    TextureSource t;
    if (raw.empty()) return t;

    if (raw[0] == '*')
    {
        const aiTexture* emb = scene->GetEmbeddedTexture(raw.c_str());
        if (!emb) return t;

        if (emb->mHeight == 0)
        {
            // Compressed blob (PNG/JPG) - mWidth is the byte count.
            t.kind = TextureSource::Kind::EmbeddedCompressed;
            const uint8_t* p = reinterpret_cast<const uint8_t*>(emb->pcData);
            t.bytes.assign(p, p + emb->mWidth);
        }
        else
        {
            // Raw ARGB8888 -> RGBA.
            t.kind   = TextureSource::Kind::EmbeddedRaw;
            t.width  = emb->mWidth;
            t.height = emb->mHeight;
            const uint32_t n = emb->mWidth * emb->mHeight;
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

    const std::string resolved = ResolveTexturePath(raw, dir);
    if (resolved.empty()) return t;
    t.kind = TextureSource::Kind::Path;
    t.path = resolved;
    return t;
}

bool GetColor(const aiMaterial* mat, const char* key, unsigned type, unsigned index, aiColor4D& out)
{
    return aiGetMaterialColor(mat, key, type, index, &out) == AI_SUCCESS;
}

}

TextureSource DescribeTexture(const aiScene* scene, const aiMaterial* mat, aiTextureType type, const std::string& dir)
{
    if (!mat || mat->GetTextureCount(type) == 0) return {};
    aiString texPath;
    if (mat->GetTexture(type, 0, &texPath) != AI_SUCCESS) return {};
    return DescribeRaw(scene, texPath.C_Str(), dir);
}

Ref<Texture2D> Realize(const TextureSource& t)
{
    switch (t.kind)
    {
        case TextureSource::Kind::Path:
            return Texture2D::Create(t.path);
        case TextureSource::Kind::Color:
        {
            auto tex = Texture2D::Create(1, 1, TextureFormat::RGBA8);
            tex->SetData(&t.color, 4);
            tex->QueueUpload();
            return tex;
        }
        case TextureSource::Kind::EmbeddedCompressed:
            return Texture2D::CreateFromMemory(t.bytes.data(), static_cast<uint32_t>(t.bytes.size()));
        case TextureSource::Kind::EmbeddedRaw:
        {
            auto tex = Texture2D::Create(t.width, t.height, TextureFormat::RGBA8);
            tex->SetData(t.bytes.data(), static_cast<uint32_t>(t.bytes.size()));
            tex->QueueUpload();
            return tex;
        }
        case TextureSource::Kind::None:
        default:
            return nullptr;
    }
}

void Write(std::ostream& os, const TextureSource& t)
{
    FileUtils::WriteValue(os, static_cast<uint32_t>(t.kind));
    switch (t.kind)
    {
        case TextureSource::Kind::Path:  FileUtils::WriteString(os, t.path); break;
        case TextureSource::Kind::Color: FileUtils::WriteValue(os, t.color); break;
        case TextureSource::Kind::EmbeddedCompressed:
            FileUtils::WriteValue(os, static_cast<uint32_t>(t.bytes.size()));
            FileUtils::WriteArray(os, t.bytes);
            break;
        case TextureSource::Kind::EmbeddedRaw:
            FileUtils::WriteValue(os, t.width);
            FileUtils::WriteValue(os, t.height);
            FileUtils::WriteValue(os, static_cast<uint32_t>(t.bytes.size()));
            FileUtils::WriteArray(os, t.bytes);
            break;
        case TextureSource::Kind::None:
        default: break;
    }
}

TextureSource ReadTextureSource(std::istream& is)
{
    TextureSource t;
    uint32_t k = 0;
    FileUtils::ReadValue(is, k);
    t.kind = static_cast<TextureSource::Kind>(k);
    switch (t.kind)
    {
        case TextureSource::Kind::Path:  t.path = FileUtils::ReadString(is); break;
        case TextureSource::Kind::Color: FileUtils::ReadValue(is, t.color);  break;
        case TextureSource::Kind::EmbeddedCompressed:
        {
            uint32_t n = 0; FileUtils::ReadValue(is, n);
            FileUtils::ReadArray(is, t.bytes, n);
            break;
        }
        case TextureSource::Kind::EmbeddedRaw:
        {
            FileUtils::ReadValue(is, t.width);
            FileUtils::ReadValue(is, t.height);
            uint32_t n = 0; FileUtils::ReadValue(is, n);
            FileUtils::ReadArray(is, t.bytes, n);
            break;
        }
        case TextureSource::Kind::None:
        default: break;
    }
    return t;
}

MaterialSource DescribeMaterial(const aiScene* scene, const aiMaterial* mat, const std::string& dir)
{
    MaterialSource m;
    if (!mat) return m;

    // glTF uses BASE_COLOR, other formats DIFFUSE.
    m.BaseColor = DescribeTexture(scene, mat, aiTextureType_BASE_COLOR, dir);
    if (!m.BaseColor.Present())
        m.BaseColor = DescribeTexture(scene, mat, aiTextureType_DIFFUSE, dir);

    aiColor4D c;
    if (GetColor(mat, AI_MATKEY_BASE_COLOR, c))
        m.BaseColorFactor = { c.r, c.g, c.b, c.a };
    else if (!m.BaseColor.Present())
    {
        m.BaseColorFactor = GetColor(mat, AI_MATKEY_COLOR_DIFFUSE, c) ? glm::vec4(c.r, c.g, c.b, c.a)
                                                                      : glm::vec4(0.7f, 0.7f, 0.7f, 1.f);
    }

    // No factors (FBX): the map alone, or dielectric without one.
    m.MetalRoughness = DescribeTexture(scene, mat, aiTextureType_METALNESS, dir);
    m.Metallic  = m.MetalRoughness.Present() ? 1.f : 0.f;
    m.Roughness = m.MetalRoughness.Present() ? 1.f : 0.5f;
    mat->Get(AI_MATKEY_METALLIC_FACTOR,  m.Metallic);
    mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, m.Roughness);

    m.Normal = DescribeTexture(scene, mat, aiTextureType_NORMALS, dir);

    m.Emissive       = DescribeTexture(scene, mat, aiTextureType_EMISSIVE, dir);
    m.EmissiveFactor = m.Emissive.Present() ? glm::vec3(1.f) : glm::vec3(0.f);
    if (GetColor(mat, AI_MATKEY_COLOR_EMISSIVE, c))
        m.EmissiveFactor = { c.r, c.g, c.b };
    return m;
}

Material Realize(const MaterialSource& s)
{
    Material m;
    m.BaseColor       = Realize(s.BaseColor);
    m.MetalRoughness  = Realize(s.MetalRoughness);
    m.Normal          = Realize(s.Normal);
    m.Emissive        = Realize(s.Emissive);
    m.BaseColorFactor = s.BaseColorFactor;
    m.Metallic        = s.Metallic;
    m.Roughness       = s.Roughness;
    m.EmissiveFactor  = s.EmissiveFactor;
    return m;
}

void Write(std::ostream& os, const MaterialSource& s)
{
    Write(os, s.BaseColor);
    Write(os, s.MetalRoughness);
    Write(os, s.Normal);
    Write(os, s.Emissive);
    FileUtils::WriteValue(os, s.BaseColorFactor);
    FileUtils::WriteValue(os, s.Metallic);
    FileUtils::WriteValue(os, s.Roughness);
    FileUtils::WriteValue(os, s.EmissiveFactor);
}

MaterialSource ReadMaterialSource(std::istream& is)
{
    MaterialSource s;
    s.BaseColor      = ReadTextureSource(is);
    s.MetalRoughness = ReadTextureSource(is);
    s.Normal         = ReadTextureSource(is);
    s.Emissive       = ReadTextureSource(is);
    FileUtils::ReadValue(is, s.BaseColorFactor);
    FileUtils::ReadValue(is, s.Metallic);
    FileUtils::ReadValue(is, s.Roughness);
    FileUtils::ReadValue(is, s.EmissiveFactor);
    return s;
}

}
