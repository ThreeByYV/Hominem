#pragma once

#include "Hominem/Core/Hominem.h"

namespace Hominem {

	class MenuLayer : public Layer
	{
	public:
		MenuLayer();

	    void OnAttach() override;
		void OnDetach() override;

		void OnUpdate(Timestep ts) override;
		void OnImGuiRender() override;
	private:
		OrthographicCameraController m_CameraController;
		Ref<Texture2D> m_BackgroundTexture;

		// Audio system
		AudioSystem m_AudioSystem;
		JobSystem m_JobSystem;
		std::future<SoundBufferHandle> m_MusicLoadFuture;
		SoundBufferHandle m_MusicBuffer = InvalidSoundBuffer;
		SoundHandle m_MusicHandle = InvalidSound;
		bool m_MusicPlaying = false;
		bool m_MusicLoadStarted = false;
		bool m_MusicLoadComplete = false;
	};

}

