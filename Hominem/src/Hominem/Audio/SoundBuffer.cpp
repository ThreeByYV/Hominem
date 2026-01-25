#include "hmnpch.h"
#include "SoundBuffer.h"

#include <fstream>
#include <cstring>

namespace Hominem {

	// WAV file format structures
	#pragma pack(push, 1)
	struct WAVHeader
	{
		char     RIFF[4];        // "RIFF"
		uint32_t ChunkSize;      // File size - 8
		char     WAVE[4];        // "WAVE"
	};

	struct WAVFmtChunk
	{
		char     fmt[4];         // "fmt "
		uint32_t SubchunkSize;   // 16 for PCM
		uint16_t AudioFormat;    // 1 for PCM
		uint16_t NumChannels;    // 1 = mono, 2 = stereo
		uint32_t SampleRate;     // e.g., 44100
		uint32_t ByteRate;       // SampleRate * NumChannels * BitsPerSample/8
		uint16_t BlockAlign;     // NumChannels * BitsPerSample/8
		uint16_t BitsPerSample;  // 8, 16, 24, or 32
	};

	struct WAVDataChunk
	{
		char     data[4];        // "data"
		uint32_t SubchunkSize;   // Number of bytes of audio data
	};
	#pragma pack(pop)

	bool SoundBufferLoader::LoadWAV(const std::string& filepath, SoundBuffer& outBuffer)
	{
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open())
		{
			HMN_CORE_ERROR("SoundBufferLoader: Cannot open file '{}'", filepath);
			return false;
		}

		// Read RIFF header
		WAVHeader header;
		file.read(reinterpret_cast<char*>(&header), sizeof(header));

		if (std::strncmp(header.RIFF, "RIFF", 4) != 0 ||
			std::strncmp(header.WAVE, "WAVE", 4) != 0)
		{
			HMN_CORE_ERROR("SoundBufferLoader: Invalid WAV file '{}'", filepath);
			return false;
		}

		// Read format chunk
		WAVFmtChunk fmt;
		file.read(reinterpret_cast<char*>(&fmt), sizeof(fmt));

		if (std::strncmp(fmt.fmt, "fmt ", 4) != 0)
		{
			HMN_CORE_ERROR("SoundBufferLoader: Missing fmt chunk in '{}'", filepath);
			return false;
		}

		if (fmt.AudioFormat != 1)
		{
			HMN_CORE_ERROR("SoundBufferLoader: Only PCM format supported (got {})", fmt.AudioFormat);
			return false;
		}

		// Skip any extra format bytes
		if (fmt.SubchunkSize > 16)
		{
			file.seekg(fmt.SubchunkSize - 16, std::ios::cur);
		}

		// Find data chunk (might be other chunks before it)
		WAVDataChunk dataChunk;
		while (file.read(reinterpret_cast<char*>(&dataChunk), sizeof(dataChunk)))
		{
			if (std::strncmp(dataChunk.data, "data", 4) == 0)
				break;

			// Skip unknown chunk
			file.seekg(dataChunk.SubchunkSize, std::ios::cur);
		}

		if (std::strncmp(dataChunk.data, "data", 4) != 0)
		{
			HMN_CORE_ERROR("SoundBufferLoader: Missing data chunk in '{}'", filepath);
			return false;
		}

		// Read audio samples
		outBuffer.FilePath = filepath;
		outBuffer.SampleRate = fmt.SampleRate;
		outBuffer.Channels = static_cast<uint8_t>(fmt.NumChannels);
		outBuffer.BitsPerSample = static_cast<uint8_t>(fmt.BitsPerSample);

		size_t numSamples = dataChunk.SubchunkSize / (fmt.BitsPerSample / 8);
		outBuffer.Samples.resize(numSamples);

		if (fmt.BitsPerSample == 16)
		{
			file.read(reinterpret_cast<char*>(outBuffer.Samples.data()),
				dataChunk.SubchunkSize);
		}
		else if (fmt.BitsPerSample == 8)
		{
			// Convert 8-bit unsigned to 16-bit signed
			std::vector<uint8_t> samples8(numSamples);
			file.read(reinterpret_cast<char*>(samples8.data()), dataChunk.SubchunkSize);

			for (size_t i = 0; i < numSamples; i++)
			{
				outBuffer.Samples[i] = (static_cast<int16_t>(samples8[i]) - 128) * 256;
			}
		}
		else
		{
			HMN_CORE_ERROR("SoundBufferLoader: Unsupported bit depth {} in '{}'",
				fmt.BitsPerSample, filepath);
			return false;
		}

		HMN_CORE_INFO("SoundBufferLoader: Loaded '{}' ({}Hz, {} channels, {} samples)",
			filepath, outBuffer.SampleRate, outBuffer.Channels, outBuffer.Samples.size());

		return true;
	}

	bool SoundBufferLoader::LoadOGG(const std::string& filepath, SoundBuffer& outBuffer)
	{
		// TODO: Implement OGG Vorbis loading using stb_vorbis or similar
		HMN_CORE_WARN("SoundBufferLoader: OGG loading not yet implemented for '{}'", filepath);
		return false;
	}

	bool SoundBufferLoader::Load(const std::string& filepath, SoundBuffer& outBuffer)
	{
		// Determine format from extension
		size_t dotPos = filepath.rfind('.');
		if (dotPos == std::string::npos)
		{
			HMN_CORE_ERROR("SoundBufferLoader: No file extension in '{}'", filepath);
			return false;
		}

		std::string ext = filepath.substr(dotPos);

		// Convert to lowercase for comparison
		for (char& c : ext)
			c = static_cast<char>(std::tolower(c));

		if (ext == ".wav")
			return LoadWAV(filepath, outBuffer);
		else if (ext == ".ogg")
			return LoadOGG(filepath, outBuffer);
		else
		{
			HMN_CORE_ERROR("SoundBufferLoader: Unsupported format '{}' in '{}'", ext, filepath);
			return false;
		}
	}

}
