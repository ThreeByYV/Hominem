#pragma once

#include "AudioBackend.h"
#include <miniaudio.h>
#include <unordered_map>

namespace Hominem {

	/// <summary>
	/// Miniaudio-based implementation of AudioBackend
	/// Uses ma_engine for high-level audio playback with support for:
	/// - MP3, WAV, FLAC, OGG formats
	/// - Volume, pitch, pan control
	/// - Looping and playback control
	/// </summary>
	class MiniaudioBackend : public AudioBackend
	{
	public:
		MiniaudioBackend();
		virtual ~MiniaudioBackend();

		bool Init(const AudioBackendConfig& config) override;
		void Shutdown() override;
		void Update() override;

		bool LoadSound(const std::string& filepath, SoundBuffer& outBuffer) override;

		bool PlaySound(const SoundBuffer& buffer, SoundInstance& instance) override;
		void StopSound(SoundInstance& instance) override;
		void PauseSound(SoundInstance& instance) override;
		void ResumeSound(SoundInstance& instance) override;

		void SetVolume(SoundInstance& instance, float volume) override;
		void SetPitch(SoundInstance& instance, float pitch) override;
		void SetPan(SoundInstance& instance, float pan) override;

		void SetMasterVolume(float volume) override;

		bool IsSoundFinished(const SoundInstance& instance) const override;

	private:
		ma_engine m_Engine;
		bool m_Initialized = false;

		// Map from buffer handle to ma_sound objects for this buffer
		std::unordered_map<SoundBufferHandle, std::vector<ma_sound*>> m_BufferSounds;
	};

}
