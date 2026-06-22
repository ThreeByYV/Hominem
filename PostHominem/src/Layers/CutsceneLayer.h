#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Scene/FreeFlyCameraController.h"
#include "Hominem/Cinematics/Cutscene.h"
#include "Hominem/Cinematics/CutsceneEditor.h"
#include "Hominem/Renderer/Skybox.h"
#include "Hominem/Assets/AssetLoaders.h"

#include "Cinematics/CutsceneDesc.h"

#include <glm/glm.hpp>

class SceneActor;

class CutsceneLayer : public Hominem::Layer
{
public:
	explicit CutsceneLayer(CutsceneDesc desc);

	void OnAttach()                                  override;
	void OnDetach()                                  override;
	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
	void OnImGuiRender()                             override;
	void OnEvent(Hominem::Event& e)                  override;

private:
	bool OnWindowResize(Hominem::WindowResizeEvent& e);
	bool OnKeyPressed(Hominem::KeyPressedEvent& e);

	void ApplyFraming();
	void UpdateFreeFlyCamera(Hominem::Timestep ts);

	CutsceneDesc m_Desc;

	// --- Live-tweakable framing (F2 panel), seeded from m_Desc on attach ---
	float     m_CamFOV    = 90.f;
	glm::vec3 m_CamPos    = {};
	glm::vec3 m_CamTarget = {};
	glm::vec3 m_SetPos    = {};
	glm::vec3 m_SetScale  = { 1.f, 1.f, 1.f };
	glm::vec3 m_SetRotDeg = {};
	float     m_SkyIntensity = 1.0f;

	Hominem::Ref<Hominem::Skybox> m_Skybox;

	SceneActor* m_Set = nullptr;

	// --- Procedural effects (live-tweakable via F4/F5) ---
	std::vector<FireInstance>  m_FireInstances;
	std::vector<SmokeInstance> m_SmokeInstances;
	float m_FireTime  = 0.f;
	float m_SmokeTime = 0.f;
	bool  m_ShowFire  = false;
	bool  m_ShowSmoke = false;
	void  FireImGui();
	void  SmokeImGui();

	// --- Async character mesh spawning ---
	std::vector<Hominem::AssetHandle<Hominem::SkinnedMesh>> m_SpawnMeshHandles;
	bool m_CharactersSpawned = false;

	// --- Free-fly debug camera (F6) ---
	Hominem::FreeFlyCameraController m_DebugFly;
	float     m_FlySpeed   = 3.0f;
	float     m_FlyYaw     = 0.0f;
	float     m_FlyPitch   = 0.0f;
	bool      m_FlyLooking = false;
	glm::vec2 m_LastMouse  = {};

	Hominem::Cutscene        m_Cutscene;
	Hominem::CutsceneContext m_Ctx;
	Hominem::CutsceneEditor  m_Editor;

	bool m_EditMode            = false;
	bool m_ShowFraming         = false;
	bool m_ShowCharacters      = false;
	bool m_TransitionRequested = false;
	bool m_ForceExit           = false;
	bool m_Transitioning       = false;
};
