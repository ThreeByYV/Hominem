#include "hmnpch.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "AudioSystem.h"
#include "Hominem/Assets/AssetLoaders.h"

#include <algorithm>

namespace Hominem {

struct SoundData
{
    ma_audio_buffer_ref BufferRef;
    ma_sound            Sound;
};

AudioSystem* AudioSystem::s_Instance = nullptr;

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

    s_Instance     = this;
    m_Config       = config;
    m_MasterVolume = config.MasterVolume;

    m_Engine = new ma_engine{};
    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.channels   = 2;
    engineConfig.sampleRate = config.SampleRate;

    if (ma_engine_init(&engineConfig, m_Engine) != MA_SUCCESS)
    {
        HMN_CORE_ERROR("AudioSystem: Failed to initialize miniaudio engine");
        delete m_Engine;
        m_Engine = nullptr;
        return false;
    }

    ma_engine_set_volume(m_Engine, config.MasterVolume);
    m_Instances.resize(config.MaxSounds);
    m_Running     = true;
    m_AudioThread = std::thread(&AudioSystem::AudioThreadFunc, this);

    m_Initialized = true;
    HMN_CORE_INFO("AudioSystem: Initialized ({}Hz, max {} sounds)", config.SampleRate, config.MaxSounds);
    return true;
}

void AudioSystem::Shutdown()
{
    if (!m_Initialized)
        return;

    HMN_CORE_INFO("AudioSystem: Shutting down...");
    s_Instance = nullptr;

    m_CommandQueue.Push(ShutdownCmd{});
    if (m_AudioThread.joinable())
        m_AudioThread.join();

    for (auto& instance : m_Instances)
    {
        if (auto* data = static_cast<SoundData*>(instance.BackendHandle))
        {
            ma_sound_uninit(&data->Sound);
            ma_audio_buffer_ref_uninit(&data->BufferRef);
            delete data;
        }
    }
    m_Instances.clear();

    ma_engine_uninit(m_Engine);
    delete m_Engine;
    m_Engine = nullptr;

    m_Initialized = false;
    HMN_CORE_INFO("AudioSystem: Shutdown complete");
}

AssetHandle<SoundBuffer> AudioSystem::LoadSound(std::string_view virtualPath)
{
    auto result = AssetManager::Load<SoundBuffer>(virtualPath);
    if (!result)
    {
        HMN_CORE_ERROR("AudioSystem: Failed to load '{}': {}", virtualPath, result.error().message);
        return {};
    }
    return *result;
}

void AudioSystem::UnloadSound(AssetHandle<SoundBuffer>& handle) { handle = {}; }

SoundHandle AudioSystem::Play(AssetHandle<SoundBuffer> handle, float volume, bool loop)
{
    return PlayEx(std::move(handle), volume, 1.0f, 0.0f, loop);
}

SoundHandle AudioSystem::PlayEx(AssetHandle<SoundBuffer> handle, float volume, float pitch, float pan, bool loop)
{
    if (!handle.IsLoaded())
        return InvalidSound;

    PlaySoundCmd cmd;
    cmd.Handle = AllocateSoundHandle();
    cmd.Buffer = handle.Get();
    cmd.Volume = volume;
    cmd.Pitch  = pitch;
    cmd.Pan    = pan;
    cmd.Loop   = loop;

    SoundHandle sh = cmd.Handle;
    m_CommandQueue.Push(std::move(cmd));
    return sh;
}

void AudioSystem::Stop(SoundHandle handle)   { m_CommandQueue.Push(StopSoundCmd{ handle }); }
void AudioSystem::Pause(SoundHandle handle)  { m_CommandQueue.Push(PauseSoundCmd{ handle }); }
void AudioSystem::Resume(SoundHandle handle) { m_CommandQueue.Push(ResumeSoundCmd{ handle }); }
void AudioSystem::StopAll()                  { m_CommandQueue.Push(StopAllCmd{}); }

void AudioSystem::SetVolume(SoundHandle handle, float volume) { m_CommandQueue.Push(SetVolumeCmd{ handle, volume }); }
void AudioSystem::SetPitch(SoundHandle handle, float pitch)   { m_CommandQueue.Push(SetPitchCmd{ handle, pitch }); }
void AudioSystem::SetPan(SoundHandle handle, float pan)       { m_CommandQueue.Push(SetPanCmd{ handle, pan }); }

void AudioSystem::SetMasterVolume(float volume)
{
    m_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
    m_CommandQueue.Push(SetMasterVolumeCmd{ m_MasterVolume.load() });
}

SoundHandle AudioSystem::AllocateSoundHandle()
{
    return m_NextSoundHandle.fetch_add(1);
}

void AudioSystem::AudioThreadFunc()
{
    HMN_CORE_INFO("AudioSystem: Audio thread started");
    while (m_Running)
    {
        AudioCommand cmd;
        while (m_CommandQueue.TryPop(cmd))
            ProcessCommand(cmd);
        MixAudio();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    HMN_CORE_INFO("AudioSystem: Audio thread stopped");
}

static void FreeSoundData(SoundData* data)
{
    ma_sound_uninit(&data->Sound);
    ma_audio_buffer_ref_uninit(&data->BufferRef);
    delete data;
}

void AudioSystem::ProcessCommand(const AudioCommand& cmd)
{
    std::visit([this](auto&& command)
    {
        using T = std::decay_t<decltype(command)>;

        if constexpr (std::is_same_v<T, PlaySoundCmd>)
        {
            if (!command.Buffer || !command.Buffer->IsValid())
            {
                HMN_CORE_ERROR("AudioSystem: PlaySoundCmd with invalid buffer");
                return;
            }
            for (auto& instance : m_Instances)
            {
                if (instance.IsPlaying)
                    continue;

                instance.Handle           = command.Handle;
                instance.Buffer           = command.Buffer;
                instance.Volume           = command.Volume;
                instance.Pitch            = command.Pitch;
                instance.Pan              = command.Pan;
                instance.IsLooping        = command.Loop;
                instance.PlaybackPosition = 0;
                instance.IsPlaying        = true;
                instance.IsPaused         = false;

                const SoundBuffer& buf = *command.Buffer;
                auto* data = new SoundData();

                if (ma_audio_buffer_ref_init(ma_format_f32, buf.Channels,
                                              buf.Samples.data(), buf.FrameCount,
                                              &data->BufferRef) != MA_SUCCESS
                 || ma_sound_init_from_data_source(m_Engine, &data->BufferRef,
                                                    MA_SOUND_FLAG_NO_SPATIALIZATION,
                                                    nullptr, &data->Sound) != MA_SUCCESS)
                {
                    HMN_CORE_ERROR("AudioSystem: Failed to set up sound (handle={})", command.Handle);
                    delete data;
                    instance.Reset();
                    return;
                }

                ma_sound_set_volume(&data->Sound, instance.Volume);
                ma_sound_set_pitch(&data->Sound, instance.Pitch);
                ma_sound_set_pan(&data->Sound, instance.Pan);
                ma_sound_set_looping(&data->Sound, instance.IsLooping ? MA_TRUE : MA_FALSE);

                if (ma_sound_start(&data->Sound) != MA_SUCCESS)
                {
                    HMN_CORE_ERROR("AudioSystem: Failed to start sound (handle={})", command.Handle);
                    FreeSoundData(data);
                    instance.Reset();
                    return;
                }

                instance.BackendHandle = data;
                m_ActiveSoundCount.fetch_add(1);
                return;
            }
            HMN_CORE_WARN("AudioSystem: No free sound slots");
        }
        else if constexpr (std::is_same_v<T, StopSoundCmd>)
        {
            for (auto& instance : m_Instances)
            {
                if (instance.Handle != command.Handle || !instance.IsPlaying)
                    continue;
                if (auto* data = static_cast<SoundData*>(instance.BackendHandle))
                {
                    ma_sound_stop(&data->Sound);
                    FreeSoundData(data);
                }
                instance.Reset();
                m_ActiveSoundCount.fetch_sub(1);
                return;
            }
        }
        else if constexpr (std::is_same_v<T, PauseSoundCmd>)
        {
            for (auto& instance : m_Instances)
            {
                if (instance.Handle != command.Handle)
                    continue;
                if (auto* data = static_cast<SoundData*>(instance.BackendHandle))
                    ma_sound_stop(&data->Sound);
                instance.IsPaused = true;
                return;
            }
        }
        else if constexpr (std::is_same_v<T, ResumeSoundCmd>)
        {
            for (auto& instance : m_Instances)
            {
                if (instance.Handle != command.Handle)
                    continue;
                if (auto* data = static_cast<SoundData*>(instance.BackendHandle))
                    ma_sound_start(&data->Sound);
                instance.IsPaused = false;
                return;
            }
        }
        else if constexpr (std::is_same_v<T, SetVolumeCmd>)
        {
            for (auto& instance : m_Instances)
            {
                if (instance.Handle != command.Handle)
                    continue;
                instance.Volume = std::clamp(command.Volume, 0.0f, 1.0f);
                if (auto* data = static_cast<SoundData*>(instance.BackendHandle))
                    ma_sound_set_volume(&data->Sound, instance.Volume);
                return;
            }
        }
        else if constexpr (std::is_same_v<T, SetPitchCmd>)
        {
            for (auto& instance : m_Instances)
            {
                if (instance.Handle != command.Handle)
                    continue;
                instance.Pitch = std::clamp(command.Pitch, 0.1f, 4.0f);
                if (auto* data = static_cast<SoundData*>(instance.BackendHandle))
                    ma_sound_set_pitch(&data->Sound, instance.Pitch);
                return;
            }
        }
        else if constexpr (std::is_same_v<T, SetPanCmd>)
        {
            for (auto& instance : m_Instances)
            {
                if (instance.Handle != command.Handle)
                    continue;
                instance.Pan = std::clamp(command.Pan, -1.0f, 1.0f);
                if (auto* data = static_cast<SoundData*>(instance.BackendHandle))
                    ma_sound_set_pan(&data->Sound, instance.Pan);
                return;
            }
        }
        else if constexpr (std::is_same_v<T, SetMasterVolumeCmd>)
        {
            ma_engine_set_volume(m_Engine, command.Volume);
        }
        else if constexpr (std::is_same_v<T, StopAllCmd>)
        {
            for (auto& instance : m_Instances)
            {
                if (!instance.IsPlaying)
                    continue;
                if (auto* data = static_cast<SoundData*>(instance.BackendHandle))
                {
                    ma_sound_stop(&data->Sound);
                    FreeSoundData(data);
                }
                instance.Reset();
                m_ActiveSoundCount.fetch_sub(1);
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
    for (auto& instance : m_Instances)
    {
        if (!instance.IsPlaying || instance.IsPaused || !instance.BackendHandle)
            continue;

        auto* data = static_cast<SoundData*>(instance.BackendHandle);
        if (ma_sound_at_end(&data->Sound) != MA_TRUE)
            continue;

        SoundHandle finished = instance.Handle;
        FreeSoundData(data);
        instance.Reset();
        m_ActiveSoundCount.fetch_sub(1);

        if (m_OnSoundFinished)
            m_OnSoundFinished(finished);
    }
}

}
