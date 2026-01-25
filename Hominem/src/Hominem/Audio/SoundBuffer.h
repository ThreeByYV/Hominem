#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "Hominem/Core/Core.h"

namespace Hominem {

	/// @brief Raw audio sample data loaded from a file.
	struct SoundBuffer
	{
		std::vector<int16_t> Samples;
		uint32_t SampleRate = 44100;
		uint8_t Channels = 2;
		uint8_t BitsPerSample = 16;
		std::string FilePath;

		float GetDurationSeconds() const
		{
			if (SampleRate == 0 || Channels == 0)
				return 0.0f;
			return static_cast<float>(Samples.size()) / (SampleRate * Channels);
		}

		bool IsValid() const { return !Samples.empty() && SampleRate > 0; }
	};

	using SoundBufferHandle = uint32_t;
	constexpr SoundBufferHandle InvalidSoundBuffer = UINT32_MAX;

	/// @brief Loads audio files into SoundBuffers.
	class SoundBufferLoader
	{
	public:
		static bool LoadWAV(const std::string& filepath, SoundBuffer& outBuffer);
		static bool LoadOGG(const std::string& filepath, SoundBuffer& outBuffer);
		static bool Load(const std::string& filepath, SoundBuffer& outBuffer);
	};

}
