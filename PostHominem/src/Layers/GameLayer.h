#pragma once

#include "Hominem/Scene/SceneLayer.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Events/MouseEvent.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Game/Player.h"
#include "Game/SideScrollerCamera.h"
#include "Game/WorldConfig.h"
#include "Game/Actors/NarrativeTextActor.h"
#include "Cinematics/IntroSequence.h"
#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/Audio/AudioSystem.h"

#include <glm/glm.hpp>
#include <memory>
#include <optional>

class SceneActor;

class GameLayer : public Hominem::SceneLayer
{
public:
	GameLayer();

	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
	void OnImGuiRender()                             override;
	void OnEvent(Hominem::Event& e)                  override;

	static inline bool s_SkipIntro = false;

	/// Virtual asset path for the in-game music — published so LoadingLayer can preload it.
	static constexpr const char* k_MusicPath = "game://Sounds/menu_music_2.mp3";

protected:
	Hominem::SceneDesc   Describe()                               override;
	void                 OnSceneReady(Hominem::SceneContext& ctx) override;
	void                 OnSceneDetach()                          override;
	void                 OnWindowResized(uint32_t w, uint32_t h) override;

private:
	bool OnKeyPressed(Hominem::KeyPressedEvent& e);
	bool OnMouseMoved(Hominem::MouseMovedEvent& e);

	void BootstrapFromAABB();

	// --- intro cutscene ---
	std::unique_ptr<IntroSequence> m_Intro;

	// --- game world ---
	WorldConfig  m_Config;
	float        m_Aspect = 1.f;
	std::optional<float> m_InitCameraX;
	std::optional<float> m_InitCameraZ;
	SideScrollerCamera   m_Camera;
	Player*      m_Player  = nullptr;
	SceneActor*  m_Scene3D = nullptr;
	NarrativeTextActor* m_NarrativeText  = nullptr;

	// --- audio ---
	Hominem::AssetHandle<Hominem::SoundBuffer> m_Music;
	Hominem::SoundHandle                       m_MusicHandle = Hominem::InvalidSound;

	// --- debug UI ---
	bool  m_ShowDebugUI     = false;
	bool  m_ShowPerfPanel   = true;
	float m_FPS             = 0.f;
	float m_FrameTimeMs     = 0.f;
	int   m_SelectedLight   = -1;
	bool  m_EnvMapEnabled   = true;
	float m_EnvMapIntensity = 1.0f;
};
