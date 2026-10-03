#pragma once

#include "Hominem/Scene/Actors/SkinnedMeshActor.h"
#include "Hominem/Scene/ViewModel.h"

#include <array>
#include <optional>

namespace Hominem {

    /// What a first-person rig is doing. Idle/Walk/Run loop; the rest play once, then Idle.
    enum class WeaponAnim : uint8_t { Equip, Idle, Walk, Run, Fire, Reload, ReloadEmpty, Inspect, Unequip, Count };

    inline constexpr const char* k_WeaponAnimNames[] =
        { "Equip", "Idle", "Walk", "Run", "Fire", "Reload", "Reload Empty", "Inspect", "Unequip" };
    static_assert(std::size(k_WeaponAnimNames) == static_cast<size_t>(WeaponAnim::Count));

    constexpr bool IsLooping(WeaponAnim a)
    {
        return a == WeaponAnim::Idle || a == WeaponAnim::Walk || a == WeaponAnim::Run;
    }

    /// First-person arms/weapon. Parented to the camera on spawn, so Position/Rotation are
    /// camera space (x right, y up, -z forward).
    class ViewModelActor : public SkinnedMeshActor
    {
    public:
        float PullIn   = 0.f;   ///< metres the fitted rig is moved toward the camera (+z)
        bool  Paused   = false; ///< holds the pose at PoseTime
        float PoseTime = 0.f;

        void OnCreate() override;
        void OnUpdate(Timestep ts) override;

        /// Swaps in a rig, places it from its own bones, and plays Equip.
        std::optional<ViewModelFit> Equip(Ref<SkinnedMesh> mesh);
        /// Places the current rig again, from its rest pose.
        std::optional<ViewModelFit> Fit();

        /// False when the rig has no clip for it. Negative fade: the state's default.
        bool Play(WeaponAnim anim, float fade = -1.f);

        WeaponAnim GetState() const        { return m_State; }
        bool       HasClip(WeaponAnim a) const { return m_Clips[(size_t)a] != nullptr; }
        /// A one-shot is playing; it can't be interrupted.
        bool       IsBusy() const          { return !IsLooping(m_State); }

    private:
        void  ResolveClips();
        void  PoseAtRest();
        float HeldTime() const; // current clip's last frame

        WeaponAnim m_State = WeaponAnim::Idle;
        std::array<const char*, (size_t)WeaponAnim::Count> m_Clips{}; // file's clip name per state
    };

}
