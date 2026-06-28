#include "hmnpch.h"
#include "SoundBuffer.h"
#include "Hominem/Core/Log.h"
#include <miniaudio.h>

namespace Hominem {

Ref<SoundBuffer> SoundBuffer::CreateFromFile(const std::string& path)
{
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder;
    if (ma_decoder_init_file(path.c_str(), &cfg, &decoder) != MA_SUCCESS)
    {
        HMN_CORE_ERROR("SoundBuffer: failed to open '{}'", path);
        return nullptr;
    }

    ma_uint64 frameCount = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);

    auto buffer        = CreateRef<SoundBuffer>();
    buffer->SampleRate = decoder.outputSampleRate;
    buffer->Channels   = decoder.outputChannels;
    buffer->FrameCount = frameCount;
    buffer->Samples.resize(static_cast<size_t>(frameCount * decoder.outputChannels));

    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(&decoder, buffer->Samples.data(), frameCount, &framesRead);
    ma_decoder_uninit(&decoder);

    if (framesRead == 0)
    {
        HMN_CORE_ERROR("SoundBuffer: no frames decoded from '{}'", path);
        return nullptr;
    }

    buffer->FrameCount = framesRead;
    buffer->Samples.resize(static_cast<size_t>(framesRead * buffer->Channels));
    HMN_CORE_INFO("SoundBuffer: decoded '{}' ({} frames, {}Hz, {} ch)",
        path, framesRead, buffer->SampleRate, buffer->Channels);
    return buffer;
}

}
