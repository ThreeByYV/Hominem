#pragma once

#include <thread>
#include <vector>
#include <atomic>

#include "Hominem/Core/Core.h"
#include "Hominem/Threading/Queue.h"
#include "Hominem/Assets/AssetHandle.h"
#include "AudioCommand.h"
#include "SoundBuffer.h"
#include "SoundInstance.h"

struct ma_engine;

namespace Hominem {

struct AudioConfig
{
    uint32_t SampleRate   = 44100;
    uint32_t BufferSize   = 1024;
    uint32_t MaxSounds    = 64;
    float    MasterVolume = 1.0f;
};

class AudioSystem
{
public:
    AudioSystem() = default;
    ~AudioSystem();

    AudioSystem(const AudioSystem&)            = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool Init(AudioConfig config = AudioConfig{});
    void Shutdown();
    bool IsInitialized() const { return m_Initialized; }

    static AudioSystem& Get() { return *s_Instance; }

    AssetHandle<SoundBuffer> LoadSound(std::string_view virtualPath);
    void UnloadSound(AssetHandle<SoundBuffer>& handle);

    SoundHandle Play(AssetHandle<SoundBuffer> handle, float volume = 1.0f, bool loop = false, float startOffsetSeconds = 0.0f);
    SoundHandle PlayEx(AssetHandle<SoundBuffer> handle, float volume, float pitch, float pan, bool loop, float startOffsetSeconds = 0.0f);

    void Stop(SoundHandle handle);
    void Pause(SoundHandle handle);
    void Resume(SoundHandle handle);
    void StopAll();

    void SetVolume(SoundHandle handle, float volume);
    void SetPitch(SoundHandle handle, float pitch);
    void SetPan(SoundHandle handle, float pan);

    void  SetMasterVolume(float volume);
    float GetMasterVolume() const { return m_MasterVolume; }

    void     SetOnSoundFinished(SoundFinishedCallback callback) { m_OnSoundFinished = std::move(callback); }
    uint32_t GetActiveSoundCount() const { return m_ActiveSoundCount.load(); }

private:
    void        AudioThreadFunc();
    void        ProcessCommand(const AudioCommand& cmd);
    void        MixAudio();
    SoundHandle AllocateSoundHandle();

    static AudioSystem* s_Instance;

    AudioConfig m_Config;
    bool        m_Initialized = false;

    ma_engine* m_Engine = nullptr; // heap-allocated so miniaudio.h stays out of this header

    std::thread         m_AudioThread;
    Queue<AudioCommand> m_CommandQueue;
    std::atomic<bool>   m_Running{ false };

    std::vector<SoundInstance> m_Instances;
    std::atomic<uint32_t>      m_NextSoundHandle{ 0 };
    std::atomic<uint32_t>      m_ActiveSoundCount{ 0 };

    std::atomic<float>    m_MasterVolume{ 1.0f };
    SoundFinishedCallback m_OnSoundFinished;
};

}
