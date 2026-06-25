#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Events/MouseEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Hominem/Cinematics/Cutscene.h"
#include "Game/Player.h"
#include "Game/SideScrollerCamera.h"
#include "Game/WorldConfig.h"
#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/Audio/AudioSystem.h"

#include <glm/glm.hpp>

class SceneActor;

class GameLayer : public Hominem::Layer
{
public:
	GameLayer();

	void OnAttach()                                  override;
	void OnDetach()                                  override;
	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
	void OnImGuiRender()                             override;
	void OnEvent(Hominem::Event& e)                  override;

	static inline bool      s_SkipIntro = false;
	static inline glm::vec3 s_EyeTarget = { 0.084f, 0.8f, 40.0f };

	/// Virtual asset path for the in-game music — published so LoadingLayer can preload it.
	static constexpr const char* k_MusicPath = "game://Sounds/menu_music_2.mp3";

private:
	bool OnWindowResize(Hominem::WindowResizeEvent& e);
	bool OnKeyPressed(Hominem::KeyPressedEvent& e);
	bool OnMouseMoved(Hominem::MouseMovedEvent& e);

	glm::vec3 ResolveEyeTarget(const glm::vec3& fallback) const;
	void      BootstrapFromAABB();

	/// Builds and plays the zoom-in/flash/transition cues once the wait timer expires.
	void StartIntroZoomIn();

	// --- intro cutscene ---
	// The wait is a plain timer (Play() isn't called yet, so nothing to delegate to);
	// once it expires, StartIntroZoomIn() builds the cues and hands off to m_IntroCutscene.
	float                    m_IntroTimer = 0.f;
	Hominem::Cutscene        m_IntroCutscene;
	Hominem::CutsceneContext m_IntroCtx;

	static constexpr glm::vec3 k_EyeTarget = { 0.084f, 0.8f, 29.0f };
	static constexpr float     k_EyeZoom   = 0.08f;
	static constexpr float     k_WaitDur   = 2.5f;
	static constexpr float     k_ZoomDur   = 0.7f;
	static constexpr float     k_FlashDur  = 0.15f;

	// --- game world ---
	WorldConfig        m_Config;
	float              m_Aspect      = 1.f;
	float              m_InitCameraX = 0.f;
	float              m_InitCameraZ = 0.f;
	SideScrollerCamera m_Camera;
	Player*            m_Player  = nullptr;
	SceneActor*        m_Scene3D = nullptr;

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
