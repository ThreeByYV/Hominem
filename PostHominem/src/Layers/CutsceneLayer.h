#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Scene/FreeFlyCameraController.h"
#include "Hominem/Cinematics/Cutscene.h"
#include "Hominem/Cinematics/CutsceneEditor.h"
#include "Hominem/Renderer/Skybox.h"

#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Core/AsyncLoad.h"

#include <glm/glm.hpp>
#include <array>

class SceneActor;
class SilhouetteCharacterActor;

/// @brief Loads the intro cutscene from JSON, plays it, then hands off to GameLayer.
class CutsceneLayer : public Hominem::Layer
{
public:
	CutsceneLayer();

	/// Path to the cutscene's 3D set mesh - also the key CutscenePreload loads in
	/// the background starting at game launch (see Main.cpp).
	static constexpr const char* k_SetMesh = "Resources/Textures/facade_san_nicolas.glb";

	void OnAttach()                                  override;
	void OnDetach()                                  override;
	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
	void OnImGuiRender()                             override;
	void OnEvent(Hominem::Event& e)                  override;

private:
	bool OnWindowResize(Hominem::WindowResizeEvent& e);
	bool OnKeyPressed(Hominem::KeyPressedEvent& e);

	static std::string CutscenePath();

	/// Push the live framing values onto the camera and the set actor.
	void ApplyFraming();

	void UpdateFreeFlyCamera(Hominem::Timestep ts);
	void SpawnCinematicCharacters(Hominem::Ref<Hominem::SkinnedMesh> runMesh,
	                              Hominem::Ref<Hominem::SkinnedMesh> idleMesh);
	void CharactersImGui();

	// --- 3D set framing (the burning town the intro plays against) ---
	float     m_CamFOV    = 120.f;
	glm::vec3 m_CamPos    = { -58.957f, 1.294f, -0.667f };
	glm::vec3 m_CamTarget = { -57.966f, 1.166f, -0.631f };
	glm::vec3 m_SetPos    = { 0.f, 0.f, 0.f };
	glm::vec3 m_SetScale  = { 2.f, 2.f, 2.f };
	glm::vec3 m_SetRotDeg = { 1.0f, -1.0f, -4.5f };

	static constexpr const char* k_SkyboxEXR = "Resources/Textures/satara_night_2k.exr";
	Hominem::Ref<Hominem::Skybox> m_Skybox;
	float                         m_SkyIntensity = 1.0f;

	/// One procedural fire "patch" placed in the 3D set (see fire_quad.glsl).
	struct FireInstance
	{
		glm::vec3 pos         { -53.35f, 6.05f, -1.55f };
		glm::vec3 rotDeg      { 0.f, 90.f, 0.f };
		glm::vec2 size        { 20.f, 20.f };
		float     intensity   = 1.5f;
		float     scrollSpeed = 0.6f;
		float     seed        = 0.f;
	};
	std::vector<FireInstance> m_FireInstances;
	float m_FireTime    = 0.f;
	bool  m_ShowFire    = false; ///< F4 opens the fire-placement panel.
	void  FireImGui();

	/// One procedural smoke "plume" placed in the 3D set (see smoke_quad.glsl).
	struct SmokeInstance
	{
		glm::vec3 pos         { -53.35f, 6.05f, -1.55f };
		glm::vec3 rotDeg      { 0.f, 90.f, 0.f };
		glm::vec2 size        { 20.f, 20.f };
		float     opacity     = 0.6f;
		float     scrollSpeed = 0.15f;
		float     seed        = 0.f;
	};
	std::vector<SmokeInstance> m_SmokeInstances;
	float m_SmokeTime    = 0.f;
	bool  m_ShowSmoke    = false; ///< F5 opens the smoke-placement panel.
	void  SmokeImGui();

	Hominem::FreeFlyCameraController m_DebugFly; ///< F6 toggles free-fly camera without opening the F2 panel.
	float     m_FlySpeed   = 3.0f;
	float     m_FlyYaw     = 0.0f;
	float     m_FlyPitch   = 0.0f;
	bool      m_FlyLooking = false; ///< RMB currently held - captures the mouse delta.
	glm::vec2 m_LastMouse  = { 0.f, 0.f };

	SceneActor* m_Set = nullptr; ///< 3D town set, owned by m_Scene.

	static constexpr const char* k_RunMesh  = "Resources/Textures/Running.fbx";
	static constexpr const char* k_IdleMesh = "Resources/Textures/Idle.fbx";
	static constexpr int k_RunnerCount = 5;
	std::array<SilhouetteCharacterActor*, k_RunnerCount> m_Runners{};
	SilhouetteCharacterActor* m_CenterGirl    = nullptr;
	SilhouetteCharacterActor* m_ForegroundArm = nullptr;
	bool m_ShowCharacters    = false; ///< F3 opens the silhouette character tweaker.
	bool m_CharactersSpawned = false;
	Hominem::AsyncLoad<Hominem::SkinnedMesh> m_RunMeshLoad;
	Hominem::AsyncLoad<Hominem::SkinnedMesh> m_IdleMeshLoad;
	Hominem::Cutscene            m_Cutscene;
	Hominem::CutsceneContext     m_Ctx;
	Hominem::CutsceneEditor      m_Editor;

	bool m_EditMode           = false; ///< F1 opens the authoring editor and suspends auto-transition.
	bool m_ShowFraming        = false; ///< F2 opens the set-framing panel and suspends auto-transition.
	bool m_TransitionRequested = false; ///< Set when the cutscene finishes.
	bool m_ForceExit          = false; ///< Esc: skip to gameplay regardless of edit mode.
	bool m_Transitioning      = false; ///< Guards against queueing GameLayer twice.
};
