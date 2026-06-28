#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "Hominem/Core/Core.h"

namespace Hominem {

/// Decoded PCM audio asset. Loaded once via AssetManager; each playback gets
/// a zero-copy ma_audio_buffer_ref into Samples.
struct SoundBuffer : public RefCounted
{
    std::vector<float> Samples;
    uint32_t           SampleRate = 0;
    uint32_t           Channels   = 0;
    uint64_t           FrameCount = 0;

    bool IsValid() const { return !Samples.empty() && SampleRate > 0; }

    static Ref<SoundBuffer> CreateFromFile(const std::string& path);
};

}
