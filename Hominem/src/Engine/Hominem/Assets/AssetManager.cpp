#include "hmnpch.h"
#include "AssetManager.h"

#include "Hominem/Threading/Queue.h"
#include "Hominem/Core/VFS.h"
#include "Hominem/Threading/ThreadPriority.h"

#include <unordered_map>
#include <shared_mutex>
#include <thread>
#include <mutex>

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

std::vector<std::pair<std::shared_ptr<AssetSlot>, std::function<void()>>> s_PendingCallbacks;
std::mutex                                                                s_CallbackMutex;

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
            if (slot->data)
            {
                slot->state.store(AssetState::Loaded, std::memory_order_release);
                HMN_CORE_INFO("AssetManager: loaded '{}'", slot->path);
            }
            else
            {
                slot->state.store(AssetState::Failed, std::memory_order_release);
                HMN_CORE_ERROR("AssetManager: loader produced no data for '{}'", slot->path);
            }
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
        if (slot->data)
            slot->state.store(AssetState::Loaded, std::memory_order_release);
        else
        {
            slot->state.store(AssetState::Failed, std::memory_order_release);
            HMN_CORE_ERROR("AssetManager: loader produced no data for '{}'", slot->path);
        }
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

void AssetManager::RegisterPendingCallback(std::shared_ptr<AssetSlot> slot, std::function<void()> fn)
{
    std::lock_guard lock(s_CallbackMutex);
    s_PendingCallbacks.emplace_back(std::move(slot), std::move(fn));
}

void AssetManager::PumpCallbacks()
{
    std::vector<std::function<void()>> ready;
    {
        std::lock_guard lock(s_CallbackMutex);
        for (auto it = s_PendingCallbacks.begin(); it != s_PendingCallbacks.end(); )
        {
            if (const AssetState state = it->first->state.load(std::memory_order_acquire);
                    state == AssetState::Loaded || state == AssetState::Failed)
            {
                ready.push_back(std::move(it->second));
                it = s_PendingCallbacks.erase(it);
            }
            else
                ++it;
        }
    }
    // Invoke outside the lock, callbacks may call back into AssetManager.
    for (auto& fn : ready) fn();
}

}
