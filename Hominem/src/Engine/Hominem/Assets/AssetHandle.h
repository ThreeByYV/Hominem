#pragma once

#include <cstdint>
#include <memory>
#include <atomic>
#include <string>
#include <string_view>
#include "Hominem/Core/Core.h"

namespace Hominem {

using AssetID = uint64_t;
constexpr AssetID k_InvalidAssetID = 0;

enum class AssetState : uint8_t { Unloaded, Loading, Loaded, Failed };

/// FNV-1a 64-bit hash of a resolved disk path — used as the cache key.
inline AssetID HashPath(std::string_view path)
{
    constexpr uint64_t k_Basis = 14695981039346656037ULL;
    constexpr uint64_t k_Prime = 1099511628211ULL;
    uint64_t hash = k_Basis;
    for (const unsigned char c : path)
        hash = (hash ^ c) * k_Prime;
    return hash == 0 ? 1 : hash;
}

/// Shared cache entry. All assets inherit RefCounted so data is Ref<RefCounted>.
struct AssetSlot
{
    std::atomic<AssetState> state { AssetState::Unloaded };
    Ref<RefCounted>         data;
    std::string             path;

    explicit AssetSlot(std::string p) : path(std::move(p)) {}

    AssetSlot(const AssetSlot&)            = delete;
    AssetSlot& operator=(const AssetSlot&) = delete;
};

/// Typed handle into the asset cache; check IsLoaded() before dereferencing.
template<typename T>
class AssetHandle
{
public:
    AssetHandle() = default;
    explicit AssetHandle(AssetID id, std::shared_ptr<AssetSlot> slot)
        : m_ID(id), m_Slot(std::move(slot)) {}

    [[nodiscard("Check IsValid() before calling Get() or dereferencing")]]
    bool IsValid()   const { return m_ID != k_InvalidAssetID && m_Slot != nullptr; }
    [[nodiscard("Check IsLoaded() before calling Get() to avoid returning null")]]
    bool IsLoaded()  const { return IsValid() && m_Slot->state.load(std::memory_order_acquire) == AssetState::Loaded; }
    [[nodiscard("Check IsFailed() to handle load errors and retry or fallback")]]
    bool IsFailed()  const { return IsValid() && m_Slot->state.load(std::memory_order_acquire) == AssetState::Failed; }
    [[nodiscard("Check IsLoading() to poll async load progress")]]
    bool IsLoading() const { return IsValid() && m_Slot->state.load(std::memory_order_acquire) == AssetState::Loading; }

    /// Returns null Ref if not yet loaded.
    [[nodiscard]] Ref<T> Get() const
    {
        if (!IsLoaded()) return nullptr;
        return Ref<T>(static_cast<T*>(m_Slot->data.get()));
    }

    AssetID GetID() const { return m_ID; }

    bool operator==(const AssetHandle& o) const { return m_ID == o.m_ID; }
    auto operator<=>(const AssetHandle& o) const { return m_ID <=> o.m_ID; }

private:
    AssetID                    m_ID   = k_InvalidAssetID;
    std::shared_ptr<AssetSlot> m_Slot; // slot shared between all handles + the cache
};

}
