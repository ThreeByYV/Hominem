#pragma once

#include "Hominem/Scene/Scene.h"
#include "Hominem/Audio/AudioSystem.h"
#include "Hominem/Assets/AssetLoaders.h"
#include <glm/glm.hpp>

namespace Hominem {

/// Passed to SceneLayer::OnSceneReady — single entry point for scene setup.
/// Type ctx. in your IDE to see everything available without reading engine headers.
class SceneContext
{
public:
    SceneContext(Scene& scene, AudioSystem& audio, uint32_t w, uint32_t h)
        : scene(scene)
        , audio(audio)
        , viewport{ w, h }
        , aspect(h > 0 ? (float)w / (float)h : 1.f)
    {}

    Scene&       scene;
    AudioSystem& audio;
    glm::uvec2   viewport;
    float        aspect;

    /// Load an asset synchronously (cache hit if preloaded by LoadingLayer).
    template<typename T>
    [[nodiscard]] auto Load(const std::string& path) const
    {
        return AssetManager::Load<T>(path);
    }

    /// Load an asset asynchronously; poll AssetHandle::IsLoaded() to check readiness.
    template<typename T>
    [[nodiscard]] auto LoadAsync(const std::string& path) const
    {
        return AssetManager::LoadAsync<T>(path);
    }

    /// Spawn an actor into the scene.
    template<typename T, typename... Args>
    T& SpawnActor(Args&&... args)
    {
        return scene.SpawnActor<T>(std::forward<Args>(args)...);
    }

    /// Trigger a runtime cubemap bake for environment reflections.
    void BakeEnvironment(glm::vec3 capturePos, float intensity = 1.f)
    {
        scene.BakeEnvironment(capturePos, intensity);
    }
};

} // namespace Hominem
