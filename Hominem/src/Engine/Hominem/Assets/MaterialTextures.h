#pragma once

#include "Hominem/Renderer/RHI/Texture.h"
#include "Hominem/Renderer/Geometry/Material.h"

#include <assimp/material.h>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

struct aiScene;

namespace Hominem {

/// Resolve a raw model texture path to an existing file, trying sibling/textures
/// dirs and .jpg/.jpeg alternates (FBX often stores absolute paths). "" if none.
std::string ResolveTexturePath(const std::string& rawPath, const std::string& baseDir);

/// 1x1 opaque white. Shared default albedo.
Ref<Texture2D> WhiteTexture();
/// 1x1 flat normal map (128,128,255) = no perturbation.
Ref<Texture2D> FlatNormalMap();

/// How to rebuild a texture without the source model; what mesh caches store.
struct TextureSource
{
    enum class Kind : uint32_t { None = 0, Path, Color, EmbeddedCompressed, EmbeddedRaw };
    Kind kind = Kind::None;
    std::string          path;
    uint32_t             color = 0;             // packed RGBA
    uint32_t             width = 0, height = 0;
    std::vector<uint8_t> bytes;

    bool Present() const { return kind != Kind::None; }
};

TextureSource  DescribeTexture(const aiScene* scene, const aiMaterial* mat, aiTextureType type, const std::string& dir);
Ref<Texture2D> Realize(const TextureSource& source);
void           Write(std::ostream& os, const TextureSource& source);
TextureSource  ReadTextureSource(std::istream& is);

/// A material before its textures load: what importers read and the static cache stores.
struct MaterialSource
{
    TextureSource BaseColor, MetalRoughness, Normal, Emissive;
    glm::vec4     BaseColorFactor{ 1.f };
    float         Metallic  = 0.f;
    float         Roughness = 0.5f;
    glm::vec3     EmissiveFactor{ 0.f };
};

/// Any format via assimp; fields a format lacks get the old defaults.
MaterialSource DescribeMaterial(const aiScene* scene, const aiMaterial* mat, const std::string& dir);
Material       Realize(const MaterialSource& source);
void           Write(std::ostream& os, const MaterialSource& source);
MaterialSource ReadMaterialSource(std::istream& is);

}
