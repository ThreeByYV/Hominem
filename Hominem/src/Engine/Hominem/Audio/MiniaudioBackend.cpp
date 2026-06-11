#include "hmnpch.h"
#include "MiniaudioBackend.h"
#include "Hominem/Core/Log.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace Hominem {

	MiniaudioBackend::MiniaudioBackend()
	{
		// Zero-initialize the engine
		memset(&m_Engine, 0, sizeof(ma_engine));
	}

	MiniaudioBackend::~MiniaudioBackend()
	{
		Shutdown();
	}

	bool MiniaudioBackend::Init(const AudioBackendConfig& config)
	{
		if (m_Initialized)
		{
			HMN_CORE_WARN("MiniaudioBackend::Init - Already initialized");
			return false;
		}

		// Configure the engine
		ma_engine_config engineConfig = ma_engine_config_init();
		engineConfig.channels = config.Channels;
		engineConfig.sampleRate = config.SampleRate;

		// Initialize the engine
		ma_result result = ma_engine_init(&engineConfig, &m_Engine);
		if (result != MA_SUCCESS)
		{
			HMN_CORE_ERROR("MiniaudioBackend::Init - Failed to initialize engine: {}", (int)result);
			return false;
		}

		// Set master volume
		ma_engine_set_volume(&m_Engine, config.MasterVolume);

		m_Initialized = true;
		HMN_CORE_INFO("MiniaudioBackend::Init - Engine initialized ({}Hz, {} channels)",
			config.SampleRate, config.Channels);

		return true;
	}

	void MiniaudioBackend::Shutdown()
	{
		if (!m_Initialized)
			return;

		// Clean up all ma_sound objects
		for (auto& [bufferHandle, sounds] : m_BufferSounds)
		{
			for (ma_sound* sound : sounds)
			{
				ma_sound_uninit(sound);
				delete sound;
			}
		}
		m_BufferSounds.clear();

		// Uninitialize the engine
		ma_engine_uninit(&m_Engine);
		m_Initialized = false;

		HMN_CORE_INFO("MiniaudioBackend::Shutdown - Engine shut down");
	}

	void MiniaudioBackend::Update()
	{
		// miniaudio handles updates internally via its own thread
		// No explicit update needed for ma_engine
	}

	bool MiniaudioBackend::LoadSound(const std::string& filepath, SoundBuffer& outBuffer)
	{
		// Use miniaudio's decoder to load the file
		ma_decoder decoder;
		ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_s16, 0, 0);

		ma_result result = ma_decoder_init_file(filepath.c_str(), &decoderConfig, &decoder);
		if (result != MA_SUCCESS)
		{
			HMN_CORE_ERROR("MiniaudioBackend::LoadSound - Failed to init decoder for '{}': {}", filepath, (int)result);
			return false;
		}

		// Get the length in PCM frames
		ma_uint64 lengthInFrames;
		result = ma_decoder_get_length_in_pcm_frames(&decoder, &lengthInFrames);
		if (result != MA_SUCCESS)
		{
			HMN_CORE_WARN("MiniaudioBackend::LoadSound - Could not determine length for '{}'", filepath);
			lengthInFrames = 0;
		}

		// Read all PCM frames into a buffer
		std::vector<int16_t> samples;
		if (lengthInFrames > 0)
		{
			size_t totalSamples = (size_t)(lengthInFrames * decoder.outputChannels);
			samples.resize(totalSamples);

			ma_uint64 framesRead = 0;
			ma_decoder_read_pcm_frames(&decoder, samples.data(), lengthInFrames, &framesRead);
			samples.resize((size_t)(framesRead * decoder.outputChannels));
		}
		else
		{
			// Unknown length, read in chunks
			const size_t chunkFrames = 4096;
			std::vector<int16_t> chunk(chunkFrames * decoder.outputChannels);

			ma_uint64 framesRead = 0;
			while (true)
			{
				ma_decoder_read_pcm_frames(&decoder, chunk.data(), chunkFrames, &framesRead);
				if (framesRead == 0)
					break;

				size_t samplesRead = (size_t)(framesRead * decoder.outputChannels);
				samples.insert(samples.end(), chunk.begin(), chunk.begin() + samplesRead);
			}
		}

		// Populate the output buffer
		outBuffer.Samples = std::move(samples);
		outBuffer.SampleRate = decoder.outputSampleRate;
		outBuffer.Channels = decoder.outputChannels;
		outBuffer.BitsPerSample = 16; // We force s16 format
		outBuffer.FilePath = filepath;

		ma_decoder_uninit(&decoder);

		HMN_CORE_INFO("MiniaudioBackend::LoadSound - Loaded '{}' ({} samples, {}Hz, {} ch)",
			filepath, outBuffer.Samples.size(), outBuffer.SampleRate, outBuffer.Channels);

		return true;
	}

	bool MiniaudioBackend::PlaySound(const SoundBuffer& buffer, SoundInstance& instance)
	{
		if (!m_Initialized)
		{
			HMN_CORE_ERROR("MiniaudioBackend::PlaySound - Not initialized");
			return false;
		}

		if (buffer.FilePath.empty())
		{
			HMN_CORE_ERROR("MiniaudioBackend::PlaySound - Buffer has no file path");
			return false;
		}

		// Allocate a new ma_sound
		ma_sound* sound = new ma_sound();
		memset(sound, 0, sizeof(ma_sound));

		// Initialize sound from file
		// MA_SOUND_FLAG_DECODE: Decode entire file into memory for low latency
		// MA_SOUND_FLAG_ASYNC: Load asynchronously (non-blocking)
		ma_uint32 flags = MA_SOUND_FLAG_DECODE;

		ma_result result = ma_sound_init_from_file(&m_Engine, buffer.FilePath.c_str(),
			flags, nullptr, nullptr, sound);

		if (result != MA_SUCCESS)
		{
			HMN_CORE_ERROR("MiniaudioBackend::PlaySound - Failed to init sound '{}': {}", buffer.FilePath, (int)result);
			delete sound;
			return false;
		}

		// Configure the sound properties
		ma_sound_set_volume(sound, instance.Volume);
		ma_sound_set_pitch(sound, instance.Pitch);
		ma_sound_set_pan(sound, instance.Pan);
		ma_sound_set_looping(sound, instance.IsLooping ? MA_TRUE : MA_FALSE);

		// Start playback
		result = ma_sound_start(sound);
		if (result != MA_SUCCESS)
		{
			HMN_CORE_ERROR("MiniaudioBackend::PlaySound - Failed to start sound: {}", (int)result);
			ma_sound_uninit(sound);
			delete sound;
			return false;
		}

		// Store the ma_sound in the instance
		instance.BackendHandle = sound;
		instance.IsPlaying = true;
		instance.IsPaused = false;

		// Track this sound for cleanup
		m_BufferSounds[instance.BufferHandle].push_back(sound);

		HMN_CORE_TRACE("MiniaudioBackend::PlaySound - Started playback of '{}'", buffer.FilePath);

		return true;
	}

	void MiniaudioBackend::StopSound(SoundInstance& instance)
	{
		if (!instance.BackendHandle)
			return;

		ma_sound* sound = static_cast<ma_sound*>(instance.BackendHandle);
		ma_sound_stop(sound);
		instance.IsPlaying = false;
	}

	void MiniaudioBackend::PauseSound(SoundInstance& instance)
	{
		if (!instance.BackendHandle)
			return;

		ma_sound* sound = static_cast<ma_sound*>(instance.BackendHandle);
		ma_sound_stop(sound); // miniaudio doesn't have pause, use stop
		instance.IsPaused = true;
	}

	void MiniaudioBackend::ResumeSound(SoundInstance& instance)
	{
		if (!instance.BackendHandle)
			return;

		ma_sound* sound = static_cast<ma_sound*>(instance.BackendHandle);
		ma_sound_start(sound);
		instance.IsPaused = false;
	}

	void MiniaudioBackend::SetVolume(SoundInstance& instance, float volume)
	{
		instance.Volume = volume;

		if (!instance.BackendHandle)
			return;

		ma_sound* sound = static_cast<ma_sound*>(instance.BackendHandle);
		ma_sound_set_volume(sound, volume);
	}

	void MiniaudioBackend::SetPitch(SoundInstance& instance, float pitch)
	{
		instance.Pitch = pitch;

		if (!instance.BackendHandle)
			return;

		ma_sound* sound = static_cast<ma_sound*>(instance.BackendHandle);
		ma_sound_set_pitch(sound, pitch);
	}

	void MiniaudioBackend::SetPan(SoundInstance& instance, float pan)
	{
		instance.Pan = pan;

		if (!instance.BackendHandle)
			return;

		ma_sound* sound = static_cast<ma_sound*>(instance.BackendHandle);
		ma_sound_set_pan(sound, pan);
	}

	void MiniaudioBackend::SetMasterVolume(float volume)
	{
		if (!m_Initialized)
			return;

		ma_engine_set_volume(&m_Engine, volume);
	}

	bool MiniaudioBackend::IsSoundFinished(const SoundInstance& instance) const
	{
		if (!instance.BackendHandle || !instance.IsPlaying)
			return true;

		ma_sound* sound = static_cast<ma_sound*>(instance.BackendHandle);
		return ma_sound_at_end(sound) == MA_TRUE;
	}

	// Factory function implementation
	Ref<AudioBackend> CreateAudioBackend()
	{
		return CreateRef<MiniaudioBackend>();
	}

}
