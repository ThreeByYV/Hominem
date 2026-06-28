#include "hmnpch.h"
#include "AudioSystem.h"

#include <algorithm>
#include <cstring>

namespace Hominem {

	AudioSystem::~AudioSystem()
	{
		if (m_Initialized)
			Shutdown();
	}

	bool AudioSystem::Init(AudioConfig config)
	{
		if (m_Initialized)
		{
			HMN_CORE_WARN("AudioSystem: Already initialized");
			return true;
		}

		m_Config = config;
		m_MasterVolume = config.MasterVolume;

		// Create and initialize audio backend
		m_Backend = CreateAudioBackend();

		AudioBackendConfig backendConfig;
		backendConfig.SampleRate = config.SampleRate;
		backendConfig.Channels = 2; // Stereo
		backendConfig.BufferSizeFrames = config.BufferSize;
		backendConfig.MasterVolume = config.MasterVolume;

		if (!m_Backend->Init(backendConfig))
		{
			HMN_CORE_ERROR("AudioSystem: Failed to initialize audio backend");
			m_Backend.reset();
			return false;
		}

		// Pre-allocate sound instance slots
		m_Instances.resize(config.MaxSounds);

		// Start audio thread
		m_Running = true;
		m_AudioThread = std::thread(&AudioSystem::AudioThreadFunc, this);

		m_Initialized = true;
		HMN_CORE_INFO("AudioSystem: Initialized (SampleRate={}, MaxSounds={})",
			config.SampleRate, config.MaxSounds);

		return true;
	}

	void AudioSystem::Shutdown()
	{
		if (!m_Initialized)
			return;

		HMN_CORE_INFO("AudioSystem: Shutting down...");

		// Wait for async music loading to complete (if still loading)
		if (m_MusicState == MusicState::Loading)
		{
			HMN_CORE_INFO("AudioSystem: Waiting for music load to complete...");
			m_MusicBuffer = m_MusicLoadFuture.get(); // Block until done
		}

		// Stop music if playing
		if (m_MusicState == MusicState::Playing || m_MusicState == MusicState::Paused)
		{
			StopMusic();
		}

		// Send shutdown command and wait for thread
		m_CommandQueue.Push(ShutdownCmd{});

		if (m_AudioThread.joinable())
			m_AudioThread.join();

		// Shutdown backend
		if (m_Backend)
		{
			m_Backend->Shutdown();
			m_Backend.reset();
		}

		// Clear all data
		m_Buffers.clear();
		m_BufferCache.clear();
		m_Instances.clear();

		m_Initialized = false;
		HMN_CORE_INFO("AudioSystem: Shutdown complete");
	}

	SoundBufferHandle AudioSystem::LoadSound(const std::string& filepath)
	{
		std::lock_guard<std::mutex> lock(m_BufferMutex);

		// Check cache first
		auto it = m_BufferCache.find(filepath);
		if (it != m_BufferCache.end())
		{
			HMN_CORE_INFO("AudioSystem: Using cached buffer for '{}'", filepath);
			return it->second;
		}

		// Load new buffer using backend
		SoundBuffer buffer;
		if (!m_Backend || !m_Backend->LoadSound(filepath, buffer))
		{
			HMN_CORE_ERROR("AudioSystem: Failed to load '{}'", filepath);
			return InvalidSoundBuffer;
		}

		SoundBufferHandle handle = static_cast<SoundBufferHandle>(m_Buffers.size());
		m_Buffers.push_back(std::move(buffer));
		m_BufferCache[filepath] = handle;

		HMN_CORE_INFO("AudioSystem: Loaded '{}' (handle={}, duration={:.2f}s)",
			filepath, handle, m_Buffers[handle].GetDurationSeconds());

		return handle;
	}

	void AudioSystem::UnloadSound(SoundBufferHandle handle)
	{
		std::lock_guard<std::mutex> lock(m_BufferMutex);

		if (handle >= m_Buffers.size())
			return;

		// Remove from cache
		const std::string& path = m_Buffers[handle].FilePath;
		m_BufferCache.erase(path);

		// Clear the buffer (but keep the slot to preserve handle validity)
		m_Buffers[handle] = SoundBuffer{};
	}

	SoundHandle AudioSystem::Play(SoundBufferHandle buffer, float volume, bool loop)
	{
		return PlayEx(buffer, volume, 1.0f, 0.0f, loop);
	}

	SoundHandle AudioSystem::PlayEx(SoundBufferHandle buffer, float volume, float pitch, float pan, bool loop)
	{
		if (buffer == InvalidSoundBuffer)
			return InvalidSound;

		SoundHandle handle = AllocateSoundHandle();

		PlaySoundCmd cmd;
		cmd.Handle = handle;
		cmd.BufferHandle = buffer;
		cmd.Volume = volume;
		cmd.Pitch = pitch;
		cmd.Pan = pan;
		cmd.Loop = loop;

		m_CommandQueue.Push(cmd);
		return handle;
	}

	void AudioSystem::Stop(SoundHandle handle)
	{
		m_CommandQueue.Push(StopSoundCmd{ handle });
	}

	void AudioSystem::Pause(SoundHandle handle)
	{
		m_CommandQueue.Push(PauseSoundCmd{ handle });
	}

	void AudioSystem::Resume(SoundHandle handle)
	{
		m_CommandQueue.Push(ResumeSoundCmd{ handle });
	}

	void AudioSystem::StopAll()
	{
		m_CommandQueue.Push(StopAllCmd{});
	}

	void AudioSystem::SetVolume(SoundHandle handle, float volume)
	{
		m_CommandQueue.Push(SetVolumeCmd{ handle, volume });
	}

	void AudioSystem::SetPitch(SoundHandle handle, float pitch)
	{
		m_CommandQueue.Push(SetPitchCmd{ handle, pitch });
	}

	void AudioSystem::SetPan(SoundHandle handle, float pan)
	{
		m_CommandQueue.Push(SetPanCmd{ handle, pan });
	}

	void AudioSystem::SetMasterVolume(float volume)
	{
		m_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
		m_CommandQueue.Push(SetMasterVolumeCmd{ m_MasterVolume });
	}

	SoundHandle AudioSystem::AllocateSoundHandle()
	{
		return m_NextSoundHandle.fetch_add(1);
	}

	void AudioSystem::AudioThreadFunc()
	{
		HMN_CORE_INFO("AudioSystem: Audio thread started");

		// TODO: Initialize platform audio device here (e.g., WASAPI, OpenAL, SDL_Audio)

		while (m_Running)
		{
			// Process all pending commands
			AudioCommand cmd;
			while (m_CommandQueue.TryPop(cmd))
			{
				ProcessCommand(cmd);
			}

			// Mix and output audio
			MixAudio();

			// Sleep to avoid busy-waiting (adjust based on buffer size)
			// In a real implementation, this would be driven by the audio device callback
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}

		// TODO: Shutdown platform audio device here

		HMN_CORE_INFO("AudioSystem: Audio thread stopped");
	}

	void AudioSystem::ProcessCommand(const AudioCommand& cmd)
	{
		std::visit([this](auto&& command) {
			using T = std::decay_t<decltype(command)>;

			if constexpr (std::is_same_v<T, PlaySoundCmd>)
			{
				// Find a free instance slot
				for (auto& instance : m_Instances)
				{
					if (!instance.IsPlaying)
					{
						// Get the buffer
						std::lock_guard<std::mutex> lock(m_BufferMutex);
						if (command.BufferHandle >= m_Buffers.size())
						{
							HMN_CORE_ERROR("AudioSystem: Invalid buffer handle");
							return;
						}

						const SoundBuffer& buffer = m_Buffers[command.BufferHandle];

						// Initialize instance
						instance.Handle = command.Handle;
						instance.BufferHandle = command.BufferHandle;
						instance.Volume = command.Volume;
						instance.Pitch = command.Pitch;
						instance.Pan = command.Pan;
						instance.IsLooping = command.Loop;
						instance.PlaybackPosition = 0;
						instance.IsPlaying = true;
						instance.IsPaused = false;

						// Start playback via backend
						if (m_Backend && m_Backend->PlaySound(buffer, instance))
						{
							m_ActiveSoundCount.fetch_add(1);
						}
						else
						{
							HMN_CORE_ERROR("AudioSystem: Backend failed to start playback");
							instance.Reset();
						}
						return;
					}
				}
				HMN_CORE_WARN("AudioSystem: No free sound slots available");
			}
			else if constexpr (std::is_same_v<T, StopSoundCmd>)
			{
				for (auto& instance : m_Instances)
				{
					if (instance.Handle == command.Handle && instance.IsPlaying)
					{
						if (m_Backend)
							m_Backend->StopSound(instance);
						instance.Reset();
						m_ActiveSoundCount.fetch_sub(1);
						return;
					}
				}
			}
			else if constexpr (std::is_same_v<T, PauseSoundCmd>)
			{
				for (auto& instance : m_Instances)
				{
					if (instance.Handle == command.Handle)
					{
						if (m_Backend)
							m_Backend->PauseSound(instance);
						instance.IsPaused = true;
						return;
					}
				}
			}
			else if constexpr (std::is_same_v<T, ResumeSoundCmd>)
			{
				for (auto& instance : m_Instances)
				{
					if (instance.Handle == command.Handle)
					{
						if (m_Backend)
							m_Backend->ResumeSound(instance);
						instance.IsPaused = false;
						return;
					}
				}
			}
			else if constexpr (std::is_same_v<T, SetVolumeCmd>)
			{
				for (auto& instance : m_Instances)
				{
					if (instance.Handle == command.Handle)
					{
						float volume = std::clamp(command.Volume, 0.0f, 1.0f);
						instance.Volume = volume;
						if (m_Backend)
							m_Backend->SetVolume(instance, volume);
						return;
					}
				}
			}
			else if constexpr (std::is_same_v<T, SetPitchCmd>)
			{
				for (auto& instance : m_Instances)
				{
					if (instance.Handle == command.Handle)
					{
						float pitch = std::clamp(command.Pitch, 0.1f, 4.0f);
						instance.Pitch = pitch;
						if (m_Backend)
							m_Backend->SetPitch(instance, pitch);
						return;
					}
				}
			}
			else if constexpr (std::is_same_v<T, SetPanCmd>)
			{
				for (auto& instance : m_Instances)
				{
					if (instance.Handle == command.Handle)
					{
						float pan = std::clamp(command.Pan, -1.0f, 1.0f);
						instance.Pan = pan;
						if (m_Backend)
							m_Backend->SetPan(instance, pan);
						return;
					}
				}
			}
			else if constexpr (std::is_same_v<T, SetMasterVolumeCmd>)
			{
				if (m_Backend)
					m_Backend->SetMasterVolume(command.Volume);
			}
			else if constexpr (std::is_same_v<T, StopAllCmd>)
			{
				for (auto& instance : m_Instances)
				{
					if (instance.IsPlaying)
					{
						if (m_Backend)
							m_Backend->StopSound(instance);
						instance.Reset();
						m_ActiveSoundCount.fetch_sub(1);
					}
				}
			}
			else if constexpr (std::is_same_v<T, ShutdownCmd>)
			{
				m_Running = false;
			}
		}, cmd);
	}

	void AudioSystem::MixAudio()
	{
		// With miniaudio backend, actual mixing happens internally
		// We just need to check for finished sounds and clean them up

		if (!m_Backend)
			return;

		// Update backend (processes any internal tasks)
		m_Backend->Update();

		// Check each instance for completion
		std::lock_guard<std::mutex> lock(m_BufferMutex);

		for (auto& instance : m_Instances)
		{
			if (!instance.IsPlaying)
				continue;

			if (instance.BufferHandle >= m_Buffers.size())
				continue;

			// Check if sound finished playing
			if (m_Backend->IsSoundFinished(instance))
			{
				SoundHandle finishedHandle = instance.Handle;
				instance.Reset();
				m_ActiveSoundCount.fetch_sub(1);

				// Notify observer
				if (m_OnSoundFinished)
				{
					// TODO: Queue this callback to be called on main thread
					// For now, we call it on audio thread (not ideal)
					m_OnSoundFinished(finishedHandle);
				}
			}
		}
	}

	void AudioSystem::LoadMusicAsync(const std::string& filepath, bool autoPlay, float volume, bool loop)
	{
		if (m_MusicState == MusicState::Loading)
		{
			HMN_CORE_WARN("AudioSystem: Music is already loading, ignoring new request");
			return;
		}

		// Stop current music if any
		if (m_MusicState == MusicState::Playing || m_MusicState == MusicState::Paused)
		{
			StopMusic();
		}

		// Store playback parameters
		m_AutoPlayOnLoad = autoPlay;
		m_MusicVolume = volume;
		m_MusicLoop = loop;
		m_MusicState = MusicState::Loading;

		HMN_CORE_INFO("AudioSystem: Starting async music load for '{}'", filepath);

		// Submit async loading job
		m_MusicLoadFuture = m_MusicJobSystem.SubmitWithResult([this, filepath]() -> SoundBufferHandle {
			HMN_CORE_INFO("AudioSystem: Loading music on worker thread...");
			return LoadSound(filepath);
		});
	}

	void AudioSystem::UpdateMusic()
	{
		// Check if async loading completed
		if (m_MusicState == MusicState::Loading)
		{
			// Non-blocking check if future is ready
			if (m_MusicLoadFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
			{
				m_MusicBuffer = m_MusicLoadFuture.get();

				if (m_MusicBuffer != InvalidSoundBuffer)
				{
					m_MusicState = MusicState::Ready;
					HMN_CORE_INFO("AudioSystem: Music loaded successfully");

					// Auto-play if requested
					if (m_AutoPlayOnLoad)
					{
						PlayMusic();
					}
				}
				else
				{
					HMN_CORE_ERROR("AudioSystem: Music loading failed");
					m_MusicState = MusicState::Idle;
				}
			}
		}
	}

	void AudioSystem::PlayMusic()
	{
		if (m_MusicState == MusicState::Ready || m_MusicState == MusicState::Paused)
		{
			if (m_MusicState == MusicState::Ready)
			{
				// Start playing from beginning
				m_MusicHandle = Play(m_MusicBuffer, m_MusicVolume, m_MusicLoop);
				if (m_MusicHandle != InvalidSound)
				{
					m_MusicState = MusicState::Playing;
					HMN_CORE_INFO("AudioSystem: Music playing");
				}
			}
			else // Paused
			{
				// Resume from pause
				Resume(m_MusicHandle);
				m_MusicState = MusicState::Playing;
				HMN_CORE_INFO("AudioSystem: Music resumed");
			}
		}
		else if (m_MusicState == MusicState::Loading)
		{
			HMN_CORE_WARN("AudioSystem: Music is still loading, cannot play yet");
		}
		else if (m_MusicState == MusicState::Playing)
		{
			HMN_CORE_WARN("AudioSystem: Music is already playing");
		}
	}

	void AudioSystem::PauseMusic()
	{
		if (m_MusicState == MusicState::Playing)
		{
			Pause(m_MusicHandle);
			m_MusicState = MusicState::Paused;
			HMN_CORE_INFO("AudioSystem: Music paused");
		}
	}

	void AudioSystem::ResumeMusic()
	{
		if (m_MusicState == MusicState::Paused)
		{
			Resume(m_MusicHandle);
			m_MusicState = MusicState::Playing;
			HMN_CORE_INFO("AudioSystem: Music resumed");
		}
	}

	void AudioSystem::StopMusic()
	{
		if (m_MusicState == MusicState::Playing || m_MusicState == MusicState::Paused)
		{
			Stop(m_MusicHandle);
			m_MusicHandle = InvalidSound;
			m_MusicState = MusicState::Ready; // Keep buffer loaded
			HMN_CORE_INFO("AudioSystem: Music stopped");
		}
	}

	//todo fix pausing
	void AudioSystem::ToggleMusic()
	{
		if (m_MusicState == MusicState::Playing)
		{
			PauseMusic();
		}
		else if (m_MusicState == MusicState::Paused || m_MusicState == MusicState::Ready)
		{
			PlayMusic();
		}
	}

}
