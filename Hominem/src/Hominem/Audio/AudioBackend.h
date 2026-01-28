#pragma once

#include "Hominem/Core/Core.h"
#include "SoundBuffer.h"
#include "SoundInstance.h"

namespace Hominem {

	/// <summary>
	/// Audio backend configuration structure
	/// Passed to Init() to configure the audio device
	/// </summary>
	struct AudioBackendConfig
	{
		uint32_t SampleRate = 44100;
		uint32_t Channels = 2;          // Stereo
		uint32_t BufferSizeFrames = 1024;
		float MasterVolume = 1.0f;
	};

	/// <summary>
	/// Allows switching between different audio implementations (miniaudio, irrKlang, OpenAL, etc.)
	/// without changing the AudioSystem high-level API
	/// </summary>
	class AudioBackend
	{
	public:
		virtual ~AudioBackend() = default;

		/// <summary>
		/// Initialize the audio backend and device
		/// </summary>
		/// <param name="config">Configuration parameters</param>
		/// <returns>True if initialization succeeded</returns>
		virtual bool Init(const AudioBackendConfig& config) = 0;

		/// <summary>
		/// Shutdown the audio backend and release resources
		/// </summary>
		virtual void Shutdown() = 0;

		/// <summary>
		/// Update the audio backend (called from audio thread)
		/// Process any pending operations, advance playback, etc.
		/// </summary>
		virtual void Update() = 0;

		/// <summary>
		/// Load a sound file into memory
		/// </summary>
		/// <param name="filepath">Path to audio file (MP3, WAV, OGG, etc.)</param>
		/// <param name="outBuffer">Output buffer to populate with decoded audio data</param>
		/// <returns>True if load succeeded</returns>
		virtual bool LoadSound(const std::string& filepath, SoundBuffer& outBuffer) = 0;

		/// <summary>
		/// Start playing a sound
		/// </summary>
		/// <param name="buffer">Sound buffer containing the audio data and file path</param>
		/// <param name="instance">Sound instance to start (contains volume, pitch, pan, etc.)</param>
		/// <returns>True if playback started successfully</returns>
		virtual bool PlaySound(const SoundBuffer& buffer, SoundInstance& instance) = 0;

		/// <summary>
		/// Stop a playing sound
		/// </summary>
		/// <param name="instance">Sound instance to stop</param>
		virtual void StopSound(SoundInstance& instance) = 0;

		/// <summary>
		/// Pause a playing sound
		/// </summary>
		/// <param name="instance">Sound instance to pause</param>
		virtual void PauseSound(SoundInstance& instance) = 0;

		/// <summary>
		/// Resume a paused sound
		/// </summary>
		/// <param name="instance">Sound instance to resume</param>
		virtual void ResumeSound(SoundInstance& instance) = 0;

		/// <summary>
		/// Set the volume of a playing sound
		/// </summary>
		/// <param name="instance">Sound instance</param>
		/// <param name="volume">Volume (0.0 - 1.0)</param>
		virtual void SetVolume(SoundInstance& instance, float volume) = 0;

		/// <summary>
		/// Set the pitch/playback speed of a sound
		/// </summary>
		/// <param name="instance">Sound instance</param>
		/// <param name="pitch">Pitch multiplier (0.1 - 4.0, 1.0 = normal speed)</param>
		virtual void SetPitch(SoundInstance& instance, float pitch) = 0;

		/// <summary>
		/// Set the stereo pan of a sound
		/// </summary>
		/// <param name="instance">Sound instance</param>
		/// <param name="pan">Pan (-1.0 = left, 0.0 = center, 1.0 = right)</param>
		virtual void SetPan(SoundInstance& instance, float pan) = 0;

		/// <summary>
		/// Set the master volume for all sounds
		/// </summary>
		/// <param name="volume">Master volume (0.0 - 1.0)</param>
		virtual void SetMasterVolume(float volume) = 0;

		/// <summary>
		/// Check if a sound instance has finished playing
		/// </summary>
		/// <param name="instance">Sound instance to check</param>
		/// <returns>True if sound has finished playing (not paused, reached end)</returns>
		virtual bool IsSoundFinished(const SoundInstance& instance) const = 0;
	};

	/// <summary>
	/// Factory function to create the default audio backend
	/// Currently creates MiniaudioBackend
	/// </summary>
	Ref<AudioBackend> CreateAudioBackend();

}
