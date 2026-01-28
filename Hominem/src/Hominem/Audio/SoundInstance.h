#pragma once

#include <cstdint>
#include <functional>

#include "SoundBuffer.h"

namespace Hominem {

	using SoundHandle = uint32_t;
	constexpr SoundHandle InvalidSound = UINT32_MAX;

	/// @brief A single playing instance of a sound.
	struct SoundInstance
	{
		SoundBufferHandle BufferHandle = InvalidSoundBuffer;
		SoundHandle Handle = InvalidSound;

		size_t PlaybackPosition = 0;
		float Volume = 1.0f;
		float Pitch = 1.0f;
		float Pan = 0.0f;

		bool IsPlaying = false;
		bool IsLooping = false;
		bool IsPaused = false;

		// Backend-specific handle (e.g., ma_sound* for miniaudio)
		void* BackendHandle = nullptr;

		bool IsActive() const { return IsPlaying && !IsPaused; }

		void Reset()
		{
			PlaybackPosition = 0;
			IsPlaying = false;
			IsPaused = false;
			BackendHandle = nullptr;
		}
	};

	using SoundFinishedCallback = std::function<void(SoundHandle)>;

}
