#pragma once

#include <thread>
#include <vector>
#include <unordered_map>
#include <atomic>

#include "Hominem/Core/Core.h"
#include "Hominem/Threading/Queue.h"
#include "Hominem/Threading/JobSystem.h"
#include "AudioCommand.h"
#include "SoundBuffer.h"
#include "SoundInstance.h"
#include "AudioBackend.h"
#include <future>

namespace Hominem {

	struct AudioConfig
	{
		uint32_t SampleRate = 44100;
		uint32_t BufferSize = 1024;
		uint32_t MaxSounds = 64;
		float MasterVolume = 1.0f;
	};

	enum class MusicState
	{
		Idle,      
		Loading,    
		Ready,    
		Playing,  
		Paused 
	};

	/// @brief Audio playback system with dedicated audio thread.
	class AudioSystem
	{
	public:
		AudioSystem() = default;
		~AudioSystem();

		AudioSystem(const AudioSystem&) = delete;
		AudioSystem& operator=(const AudioSystem&) = delete;

		bool Init(AudioConfig config = AudioConfig{});
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

		// Global
		void SetMasterVolume(float volume);
		float GetMasterVolume() const { return m_MasterVolume; }

		// Callbacks
		void SetOnSoundFinished(SoundFinishedCallback callback) { m_OnSoundFinished = callback; }

		uint32_t GetActiveSoundCount() const { return m_ActiveSoundCount.load(); }

		// High-level music management
		void LoadMusicAsync(const std::string& filepath, bool autoPlay = true, float volume = 1.0f, bool loop = true);
		void PlayMusic();
		void PauseMusic();
		void ResumeMusic();
		void StopMusic();
		void ToggleMusic();
		void UpdateMusic(); // Call each frame to process async loading

		MusicState GetMusicState() const { return m_MusicState; }
		bool IsMusicLoaded() const { return m_MusicState != MusicState::Idle && m_MusicState != MusicState::Loading; }
		bool IsMusicPlaying() const { return m_MusicState == MusicState::Playing; }

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

		// High-level music playback state
		MusicState m_MusicState = MusicState::Idle;
		JobSystem m_MusicJobSystem;
		std::future<SoundBufferHandle> m_MusicLoadFuture;
		SoundBufferHandle m_MusicBuffer = InvalidSoundBuffer;
		SoundHandle m_MusicHandle = InvalidSound;
		bool m_AutoPlayOnLoad = false;
		float m_MusicVolume = 1.0f;
		bool m_MusicLoop = true;
	};

}
