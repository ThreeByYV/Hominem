#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/Actors/ViewModelActor.h"

#include <string>
#include <vector>

namespace Hominem {

    struct FirstPersonWeapon
    {
        std::string Name;
        std::string Path; ///< skinned rig, e.g. "game://Weapons/9mm.glb"
    };

    /// A first-person player: carries weapons and turns input into what the view model plays.
    /// LMB fire, R reload (Shift+R empty), F inspect, W walk, Shift+W run.
    class FirstPersonPlayerActor : public Actor
    {
    public:
        /// `pullIn`: metres every rig sits closer to the camera than the fit puts it.
        explicit FirstPersonPlayerActor(std::vector<FirstPersonWeapon> weapons, float pullIn = 0.f)
            : m_Weapons(std::move(weapons)), m_PullIn(pullIn) {}

        bool InputEnabled = true;

        void OnCreate() override;          // spawns the view model, equips the first weapon
        void OnUpdate(Timestep ts) override;

        /// False if the rig didn't load.
        bool Equip(size_t index);

        ViewModelActor*                       GetViewModel()   { return m_ViewModel; }
        const std::vector<FirstPersonWeapon>& GetWeapons() const { return m_Weapons; }
        size_t                                GetWeaponIndex() const { return m_WeaponIndex; }

    private:
        void HandleInput();

        std::vector<FirstPersonWeapon> m_Weapons;
        float                          m_PullIn      = 0.f;
        size_t                         m_WeaponIndex = 0;
        ViewModelActor*                m_ViewModel   = nullptr; ///< owned by the scene
    };

}
