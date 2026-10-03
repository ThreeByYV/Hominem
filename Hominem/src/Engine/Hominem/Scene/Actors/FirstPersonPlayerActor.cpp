#include "hmnpch.h"
#include "FirstPersonPlayerActor.h"

#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Core/MouseButtonCodes.h"
#include "Hominem/Scene/Scene.h"

#include <imgui.h>

namespace Hominem {

void FirstPersonPlayerActor::OnCreate()
{
    m_ViewModel         = &m_Scene->SpawnActor<ViewModelActor>();
    m_ViewModel->PullIn = m_PullIn;
    if (!m_Weapons.empty()) Equip(0);
}

bool FirstPersonPlayerActor::Equip(size_t index)
{
    if (index >= m_Weapons.size()) return false;
    const FirstPersonWeapon& weapon = m_Weapons[index];

    const auto r = AssetManager::Load<SkinnedMesh>(weapon.Path);
    Ref<SkinnedMesh> mesh = r ? r->Get() : nullptr;
    if (!mesh)
    {
        HMN_CORE_ERROR("FirstPersonPlayer: couldn't load {}", weapon.Path);
        return false;
    }
    m_WeaponIndex = index;

    const auto fit = m_ViewModel->Equip(mesh);
    const glm::vec3& p = m_ViewModel->Position;
    if (fit)
        HMN_CORE_INFO("FirstPersonPlayer: {} fitted: eye from {}, facing from {}, at ({:.3f}, {:.3f}, {:.3f})",
                      weapon.Name, fit->EyeSource, fit->FacingSource, p.x, p.y, p.z);
    else
        HMN_CORE_WARN("FirstPersonPlayer: {} has no camera/eye/head bones, placed 1 m ahead", weapon.Name);

    for (size_t i = 0; i < (size_t)WeaponAnim::Count; i++)
        if (!m_ViewModel->HasClip((WeaponAnim)i))
            HMN_CORE_WARN("FirstPersonPlayer: {} has no clip for '{}'", weapon.Name, k_WeaponAnimNames[i]);
    return true;
}

void FirstPersonPlayerActor::OnUpdate(Timestep)
{
    if (InputEnabled && m_ViewModel && m_ViewModel->Mesh && !m_ViewModel->Paused)
        HandleInput();
}

void FirstPersonPlayerActor::HandleInput()
{
    // One-shots run to the end before anything else can start.
    if (m_ViewModel->IsBusy()) return;

    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && Input::IsMouseButtonPressed(HMN_MOUSE_BUTTON_LEFT))
    {
        m_ViewModel->Play(WeaponAnim::Fire);
        return;
    }
    if (io.WantCaptureKeyboard) return;

    if (Input::IsKeyPressed(HMN_KEY_R))
    {
        const bool empty = Input::IsKeyPressed(HMN_KEY_LEFT_SHIFT); // stand-in until there's ammo
        m_ViewModel->Play(empty ? WeaponAnim::ReloadEmpty : WeaponAnim::Reload);
        return;
    }
    if (Input::IsKeyPressed(HMN_KEY_F))
    {
        m_ViewModel->Play(WeaponAnim::Inspect);
        return;
    }

    // Only restart a loop clip when the state changes.
    const bool       moving = Input::IsKeyPressed(HMN_KEY_W);
    const WeaponAnim loop   = !moving ? WeaponAnim::Idle
                            : Input::IsKeyPressed(HMN_KEY_LEFT_SHIFT) ? WeaponAnim::Run : WeaponAnim::Walk;
    if (loop != m_ViewModel->GetState())
        m_ViewModel->Play(loop);
}

}
