#pragma once

#include <thread>
#include <vector>
#include <unordered_map>
#include <atomic>

#include "Hominem/Core/Core.h"
#include "Hominem/Threading/Queue.h"
#include "AudioCommand.h"
#include "SoundBuffer.h"
#include "SoundInstance.h"
#include "AudioBackend.h"

namespace Hominem {

	struct AudioConfig
	{
		uint32_t SampleRate = 44100;
		uint32_t BufferSize = 1024;
		uint32_t MaxSounds = 64;
		float MasterVolume = 1.0f;
	};

	/// @brief Audio playback system with dedicated audio thread.
	class AudioSystem
	{
	public:
		AudioSystem() = default;
		~AudioSystem();

		AudioSystem(const AudioSystem&) = delete;
		AudioSystem& operator=(const AudioSystem&) = delete;

		bool Init(const AudioConfig& config = {});
		void Shutdown();
		bool IsInitialized() const { return m_Initialized; }

		// Sound loading
		SoundBufferHandle LoadSound(const std::string& filepath);
		void UnloadSound(SoundBufferHandle handle);

		// Playback
		SoundHandle Play(SoundBufferHandle buffer, float volume = 1.0f, bool loop = false);
		SoundHandle PlayEx(SoundBufferHandle buffer, float volume, float pitch, float pan, bool loop);
		
		void Stop(SoundHandle handle);
		void Pause(SoundHandle handle);
		void Resume(SoundHandle handle);
		void StopAll();

		// Properties
		void SetVolume(SoundHandle handle, float volume);
		void SetPitch(SoundHandle handle, float pitch);
		void SetPan(SoundHandle handle, float pan);

		// 3D audio
		void SetPosition(SoundHandle handle, const glm::vec3& position);
		void SetListenerPosition(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);

		// Global
		void SetMasterVolume(float volume);
		float GetMasterVolume() const { return m_MasterVolume; }

		// Callbacks
		void SetOnSoundFinished(SoundFinishedCallback callback) { m_OnSoundFinished = callback; }

		uint32_t GetActiveSoundCount() const { return m_ActiveSoundCount.load(); }

	private:
		void AudioThreadFunc();
		void ProcessCommand(const AudioCommand& cmd);
		void MixAudio();
		SoundHandle AllocateSoundHandle();

	private:
		AudioConfig m_Config;
		bool m_Initialized = false;

		Ref<AudioBackend> m_Backend;

		std::thread m_AudioThread;
		Queue<AudioCommand> m_CommandQueue;
		std::atomic<bool> m_Running{ false };

		std::vector<SoundBuffer> m_Buffers;
		std::unordered_map<std::string, SoundBufferHandle> m_BufferCache;
		std::mutex m_BufferMutex;

		std::vector<SoundInstance> m_Instances;
		std::atomic<uint32_t> m_NextSoundHandle{ 0 };
		std::atomic<uint32_t> m_ActiveSoundCount{ 0 };

		std::atomic<float> m_MasterVolume{ 1.0f };
		SoundFinishedCallback m_OnSoundFinished;

		glm::vec3 m_ListenerPosition{ 0.0f };
		glm::vec3 m_ListenerForward{ 0.0f, 0.0f, -1.0f };
		glm::vec3 m_ListenerUp{ 0.0f, 1.0f, 0.0f };
	};

}
