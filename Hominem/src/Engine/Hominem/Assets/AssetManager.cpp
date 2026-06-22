#include "hmnpch.h"
#include "AssetManager.h"

#include "Hominem/Threading/Queue.h"
#include "Hominem/Core/VFS.h"
#include "Hominem/Threading/ThreadPriority.h"

#include <unordered_map>
#include <shared_mutex>
#include <thread>

namespace Hominem {

namespace {

struct LoadRequest
{
    std::shared_ptr<AssetSlot>    slot;
    AssetLoaderFn                 loader;
};

std::unordered_map<AssetID, std::shared_ptr<AssetSlot>> s_Store;
std::shared_mutex                                s_StoreMutex;

Queue<LoadRequest>  s_LoadQueue;
std::jthread        s_LoaderThread;

void LoaderThreadFunc()
{
    SetCurrentThreadPriorityLow();
    HMN_CORE_INFO("AssetManager: loader thread started");
    while (true)
    {
        auto req = s_LoadQueue.WaitAndPop();
        if (!req) break; // queue shut down

        auto& slot = req->slot;
        slot->state.store(AssetState::Loading, std::memory_order_release);
        HMN_CORE_INFO("AssetManager: loading '{}'", slot->path);

        try
        {
            slot->data = req->loader(slot->path);
            slot->state.store(slot->data ? AssetState::Loaded : AssetState::Failed,
                              std::memory_order_release);
            HMN_CORE_INFO("AssetManager: loaded '{}'", slot->path);
        }
        catch (const std::exception& e)
        {
            slot->state.store(AssetState::Failed, std::memory_order_release);
            HMN_CORE_ERROR("AssetManager: exception loading '{}': {}", slot->path, e.what());
        }
    }
    HMN_CORE_INFO("AssetManager: loader thread stopped");
}

} // namespace

void AssetManager::Init()
{
    s_LoaderThread = std::jthread(LoaderThreadFunc);
    HMN_CORE_INFO("AssetManager: initialized");
}

void AssetManager::Shutdown()
{
    s_LoadQueue.Shutdown(); // wakes the loader thread, WaitAndPop returns nullopt
    s_LoaderThread = {};    // jthread joins on destruct

    std::unique_lock lock(s_StoreMutex);
    s_Store.clear();
    HMN_CORE_INFO("AssetManager: shutdown");
}

std::string AssetManager::ResolvePath(std::string_view virtualPath)
{
    std::string resolved = VFS::Resolve(std::string(virtualPath));
    return resolved.empty() ? std::string(virtualPath) : resolved;
}

void AssetManager::EvictUnused()
{
    std::unique_lock lock(s_StoreMutex);
    for (auto it = s_Store.begin(); it != s_Store.end(); )
    {
        // use_count == 1 means only the cache holds this slot (no external handles)
        if (it->second.use_count() == 1)
        {
            HMN_CORE_INFO("AssetManager: evicting '{}'", it->second->path);
            it = s_Store.erase(it);
        }
        else
            ++it;
    }
}

void AssetManager::Evict(AssetID id)
{
    std::unique_lock lock(s_StoreMutex);
    if (const auto it = s_Store.find(id); it != s_Store.end())
    {
        HMN_CORE_INFO("AssetManager: force evicting '{}'", it->second->path);
        s_Store.erase(it);
    }
}

std::shared_ptr<AssetSlot> AssetManager::GetOrCreateSlot(AssetID id, std::string_view resolvedPath)
{
    // Fast path: read lock
    {
        std::shared_lock lock(s_StoreMutex);
        if (auto it = s_Store.find(id); it != s_Store.end())
            return it->second;
    }
    // Slow path: write lock
    std::unique_lock lock(s_StoreMutex);
    if (auto it = s_Store.find(id); it != s_Store.end())
        return it->second;

    auto slot = std::make_shared<AssetSlot>(std::string(resolvedPath));
    s_Store.emplace(id, slot);
    return slot;
}

void AssetManager::LoadSync(std::shared_ptr<AssetSlot> slot, const AssetLoaderFn& loader)
{
    slot->state.store(AssetState::Loading, std::memory_order_release);
    HMN_CORE_INFO("AssetManager: loading (sync) '{}'", slot->path);

    try {
        auto data = loader(slot->path);
        slot->data = std::move(data);
        slot->state.store(slot->data ? AssetState::Loaded : AssetState::Failed,
                          std::memory_order_release);
    }
    catch (const std::exception& e) {
        slot->state.store(AssetState::Failed, std::memory_order_release);
        HMN_CORE_ERROR("AssetManager: exception loading '{}': {}", slot->path, e.what());
    }
}

void AssetManager::EnqueueAsync(std::shared_ptr<AssetSlot> slot, AssetLoaderFn loader)
{
    // Mark as loading immediately so a second call doesn't enqueue again
    AssetState expected = AssetState::Unloaded;
    if (!slot->state.compare_exchange_strong(expected, AssetState::Loading,
                                              std::memory_order_acq_rel))
        return; // another thread already picked this up

    s_LoadQueue.Push(LoadRequest{ std::move(slot), std::move(loader) });
}

}
