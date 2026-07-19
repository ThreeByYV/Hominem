#pragma once

#include "AssetManager.h"
#include "MeshData.h"
#include "StaticMeshImporter.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Renderer/Skybox.h"
#include "Hominem/Renderer/ShaderSource.h"
#include "Hominem/Audio/SoundBuffer.h"

#include <fstream>

namespace Hominem {

template<> struct AssetLoader<Texture2D> {
    static Ref<Texture2D> Load(const std::string& path) { return Texture2D::Create(path); }
};

template<> struct AssetLoader<StaticMesh> {
    static Ref<StaticMesh> Load(const std::string& path) {
        auto mesh = StaticMesh::Create();
        if (!mesh->LoadFromFile(path)) return nullptr;
        return mesh;
    }
};

template<> struct AssetLoader<SkinnedMesh> {
    static Ref<SkinnedMesh> Load(const std::string& path) {
        auto mesh = SkinnedMesh::Create();
        if (!mesh->LoadFromFile(path)) return nullptr;
        return mesh;
    }
};

template<> struct AssetLoader<Font> {
    static Ref<Font> Load(const std::string& path) { return CreateRef<Font>(path); }
};

template<> struct AssetLoader<Skybox> {
    static Ref<Skybox> Load(const std::string& path) { return Skybox::CreateFromEquirectEXR(path); }
};

template<> struct AssetLoader<SoundBuffer> {
    static Ref<SoundBuffer> Load(const std::string& path) { return SoundBuffer::CreateFromFile(path); }
};

template<> struct AssetLoader<StaticMeshData> {
    static Ref<StaticMeshData> Load(const std::string& path) {
        auto result = ImportStaticMesh(path);
        if (!result) {
            HMN_CORE_ERROR("AssetLoader<StaticMeshData>: {}", result.error());
            return nullptr;
        }
        return CreateRef<StaticMeshData>(std::move(*result));
    }
};

template<> struct AssetLoader<ShaderSource> {
    static Ref<ShaderSource> Load(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open()) return nullptr;
        return CreateRef<ShaderSource>(std::string{ std::istreambuf_iterator<char>(f), {} });
    }
};

} // namespace Hominem
