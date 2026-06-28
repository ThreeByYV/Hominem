#pragma once
// Specializations of AssetLoader<T> for all engine asset types.
// Include this wherever you call AssetManager::Load<T> or LoadAsync<T>.

#include "AssetManager.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Renderer/Skybox.h"
#include "Hominem/Audio/SoundBuffer.h"

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

} // namespace Hominem
