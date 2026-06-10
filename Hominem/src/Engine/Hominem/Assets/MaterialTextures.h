#pragma once

#include "Hominem/Renderer/Texture.h"

#include <assimp/material.h>
#include <string>

struct aiScene;

namespace Hominem {

/// Resolve a raw model texture path to an existing file, trying sibling/textures
/// dirs and .jpg/.jpeg alternates (FBX often stores absolute paths). "" if none.
std::string ResolveTexturePath(const std::string& rawPath, const std::string& baseDir);

/// Decode a "*N" embedded texture (GLB). nullptr if not embedded. Auto-uploads.
Ref<Texture2D> LoadEmbeddedTexture(const aiScene* scene, const std::string& rawPath);

/// 1x1 solid-colour texture from a material colour. Auto-uploads.
Ref<Texture2D> MakeColorTexture(const aiColor4D& color);

/// Load a material's first texture of `type`, embedded or on-disk. nullptr if absent.
Ref<Texture2D> LoadMaterialTexture(const aiScene* scene, const aiMaterial* mat,
                                   aiTextureType type, const std::string& baseDir);

}
