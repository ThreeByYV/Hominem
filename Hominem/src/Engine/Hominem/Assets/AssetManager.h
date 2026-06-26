#pragma once

#include "AssetHandle.h"
#include "Hominem/Core/Core.h"

#include <expected>
#include <string>
#include <string_view>
#include <functional>

namespace Hominem {

template<typename T>
struct AssetLoader; // no default, unspecialized types will fail to compile cleanly

template<typename T>
concept Loadable = IsRefCounted<T> && requires(const std::string& path) {
    { AssetLoader<T>::Load(path) } -> std::convertible_to<Ref<T>>;
};

enum class AssetError { NotFound, LoadFailed, InvalidPath };

struct AssetErrorInfo
{
    AssetError  code;
    std::string message;
};

using AssetLoaderFn = std::function<Ref<RefCounted>(const std::string&)>;

class AssetManager
{
public:
    static void Init();
    static void Shutdown();

    static std::string ResolvePath(std::string_view virtualPath);

    template<Loadable T>
    [[nodiscard("dropping AssetHandle immediately releases the asset")]]
    static std::expected<AssetHandle<T>, AssetErrorInfo> Load(std::string_view virtualPath);


    template<Loadable T>
    [[nodiscard("dropping AssetHandle immediately releases the asset")]]
    static AssetHandle<T> LoadAsync(std::string_view virtualPath);

    /// Same as LoadAsync, but invokes onLoaded(handle) once the load finishes (Loaded or
    /// Failed) instead of requiring the caller to poll IsLoaded() every frame. The callback
    /// fires on the main thread inside PumpCallbacks(), never on the loader thread.
    template<Loadable T>
    static AssetHandle<T> LoadAsync(std::string_view virtualPath, std::function<void(AssetHandle<T>)> onLoaded);

    static void EvictUnused();

    static void Evict(AssetID id);

    /// Drains completed-load callbacks registered via the LoadAsync(path, onLoaded) overload.
    /// Call once per frame from the main thread (Application::Run).
    static void PumpCallbacks();

private:
    static std::shared_ptr<AssetSlot> GetOrCreateSlot(AssetID id, std::string_view resolvedPath);
    static void                       LoadSync(std::shared_ptr<AssetSlot> slot, const AssetLoaderFn& loader);
    static void                       EnqueueAsync(std::shared_ptr<AssetSlot> slot, AssetLoaderFn loader);
    static void                       RegisterPendingCallback(std::shared_ptr<AssetSlot> slot, std::function<void()> fn);
};

template<Loadable T>
std::expected<AssetHandle<T>, AssetErrorInfo> AssetManager::Load(std::string_view virtualPath)
{
    const std::string resolved = ResolvePath(virtualPath);
    if (resolved.empty())
        return std::unexpected(AssetErrorInfo{ AssetError::InvalidPath,
            std::string("No mount point matches: ") + std::string(virtualPath) });

    AssetID id   = HashPath(resolved);
    auto    slot = GetOrCreateSlot(id, resolved);

    if (slot->state.load(std::memory_order_acquire) != AssetState::Loaded)
    {
        LoadSync(slot, [](const std::string& path) -> Ref<RefCounted> {
            return AssetLoader<T>::Load(path);
        });
    }

    if (slot->state.load(std::memory_order_acquire) == AssetState::Failed)
        return std::unexpected(AssetErrorInfo{ AssetError::LoadFailed,
            std::string("Failed to load: ") + resolved });

    return AssetHandle<T>{ id, slot };
}

template<Loadable T>
AssetHandle<T> AssetManager::LoadAsync(std::string_view virtualPath)
{
    std::string resolved = ResolvePath(virtualPath);
    if (resolved.empty())
        return {};

    AssetID id   = HashPath(resolved);
    auto    slot = GetOrCreateSlot(id, resolved);

    if (slot->state.load(std::memory_order_acquire) == AssetState::Unloaded)
    {
        EnqueueAsync(slot, [](const std::string& path) -> Ref<RefCounted> {
            return AssetLoader<T>::Load(path);
        });
    }

    return AssetHandle<T>{ id, slot };
}

template<Loadable T>
AssetHandle<T> AssetManager::LoadAsync(std::string_view virtualPath, std::function<void(AssetHandle<T>)> onLoaded)
{
    AssetHandle<T> handle = LoadAsync<T>(virtualPath);
    if (onLoaded && handle.IsValid())
    {
        auto slot = GetOrCreateSlot(handle.GetID(), ResolvePath(virtualPath));
        RegisterPendingCallback(slot, [handle, cb = std::move(onLoaded)] { cb(handle); });
    }
    return handle;
}

}
