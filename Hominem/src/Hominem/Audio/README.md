# Audio System Documentation

## Overview

The Hominem audio system uses a **Strategy/Bridge pattern** to abstract audio backends, making it easy to swap implementations. Currently integrated with **miniaudio** for cross-platform audio playback.

## Architecture

### Components

1. **AudioBackend (Interface)** - `AudioBackend.h`
   - Abstract interface defining audio operations
   - Allows swapping between miniaudio, irrKlang, OpenAL, etc.
   - Uses GoF Strategy/Bridge pattern

2. **MiniaudioBackend (Implementation)** - `MiniaudioBackend.h/.cpp`
   - Concrete implementation using miniaudio library
   - Supports MP3, WAV, OGG, FLAC formats out of the box
   - Handles pitch, pan, volume control

3. **AudioSystem (High-level API)** - `AudioSystem.h/.cpp`
   - Thread-safe command queue pattern
   - Dedicated audio thread for processing
   - Integrates with JobSystem for async operations

4. **Supporting Types**
   - `SoundBuffer.h` - PCM audio data container
   - `SoundInstance.h` - Playing sound state
   - `AudioCommand.h` - Command pattern for thread communication

### Threading Model

```
Main Thread                          Audio Thread
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
AudioSystem::Play()
    │
    └─> Push(PlaySoundCmd) ────────────┐
                                        │
                                   ProcessCommand()
                                        │
                                   Backend->PlaySound()
                                        │
                                   MixAudio()
                                        │
                                   Backend->Update()
```

- **Main Thread**: Submits commands (Play, Stop, SetVolume, etc.)
- **Audio Thread**: Processes commands and manages playback
- **Backend**: Handles actual device I/O (runs its own threads internally)

## Usage

### Basic Setup

```cpp
#include "Hominem/Audio/AudioSystem.h"

// Initialize
AudioSystem audioSystem;
audioSystem.Init();

// Load sound files (supports MP3, WAV, OGG, FLAC)
SoundBufferHandle music = audioSystem.LoadSound("assets/music.mp3");
SoundBufferHandle sfx = audioSystem.LoadSound("assets/jump.wav");

// Play sounds
SoundHandle musicHandle = audioSystem.Play(music, 0.5f, true);  // volume=0.5, loop=true
SoundHandle sfxHandle = audioSystem.Play(sfx, 1.0f, false);      // volume=1.0, loop=false

// Control playback
audioSystem.SetVolume(musicHandle, 0.8f);
audioSystem.SetPitch(sfxHandle, 1.2f);  // Play 20% faster
audioSystem.Pause(musicHandle);
audioSystem.Resume(musicHandle);
audioSystem.Stop(musicHandle);

// Cleanup
audioSystem.Shutdown();
```

### Advanced Features

#### Stereo Panning

```cpp
audioSystem.SetPan(soundHandle, -1.0f);  // Full left
audioSystem.SetPan(soundHandle, 0.0f);   // Center
audioSystem.SetPan(soundHandle, 1.0f);   // Full right
```

#### Pitch Control

```cpp
audioSystem.SetPitch(soundHandle, 0.5f);  // Half speed (deeper)
audioSystem.SetPitch(soundHandle, 2.0f);  // Double speed (higher pitch)
// Range: 0.1 - 4.0
```

#### Sound Finished Callback

```cpp
audioSystem.SetOnSoundFinished([](SoundHandle handle) {
    HMN_CORE_INFO("Sound {} finished playing", handle);
});
```

#### Async Loading with JobSystem

```cpp
#include "Hominem/Threading/JobSystem.h"

JobSystem jobSystem;

// Load sound asynchronously
auto future = jobSystem.SubmitWithResult<SoundBufferHandle>([&audioSystem]() {
    return audioSystem.LoadSound("large_music_file.mp3");
});

// Do other work...

// Get result when ready
SoundBufferHandle buffer = future.get();
if (buffer != InvalidSoundBuffer) {
    audioSystem.Play(buffer, 1.0f, true);
}
```

## Integration in Layers

See `SandboxLayer.h` for a complete example. Key points:

1. **OnAttach**: Initialize AudioSystem and load sounds
2. **OnUpdate**: Handle input for audio control
3. **OnDetach**: Shutdown AudioSystem
4. **OnImGuiRender**: Display audio debug info

```cpp
class MyLayer : public Layer
{
    void OnAttach() override
    {
        m_AudioSystem.Init();
        m_Music = m_AudioSystem.LoadSound("assets/music.mp3");
    }

    void OnUpdate(Timestep ts) override
    {
        if (Input::IsKeyPressed(HMN_KEY_M))
            m_AudioSystem.Play(m_Music, 0.7f, true);
    }

    void OnDetach() override
    {
        m_AudioSystem.Shutdown();
    }

private:
    AudioSystem m_AudioSystem;
    SoundBufferHandle m_Music = InvalidSoundBuffer;
};
```

## Supported Audio Formats

miniaudio backend supports:
- **MP3** - MPEG-1/2 Layer III
- **WAV** - PCM, ADPCM
- **OGG** - Vorbis
- **FLAC** - Free Lossless Audio Codec

## Configuration

```cpp
AudioConfig config;
config.SampleRate = 44100;      // CD quality (default)
config.BufferSize = 1024;       // Frames per buffer
config.MaxSounds = 64;          // Max simultaneous sounds
config.MasterVolume = 1.0f;     // Global volume

audioSystem.Init(config);
```

## Thread Safety

- ✅ **Thread-safe**: LoadSound, Play, Stop, Pause, Resume, SetVolume, SetPitch, SetPan, SetMasterVolume
- ✅ **Lock-free**: Command submission (atomic operations)
- ✅ **No stalls**: Main thread never blocks on audio thread

## Performance Characteristics

- **Latency**: ~5ms command processing latency
- **Memory**: Each sound instance: ~100 bytes
- **CPU**: Audio thread runs at ~200 Hz polling rate
- **Backend**: miniaudio runs its own high-priority device thread

## Adding New Backends

To add a new audio backend (e.g., irrKlang, OpenAL):

1. Create `NewBackend.h/.cpp` inheriting from `AudioBackend`
2. Implement all pure virtual methods
3. Update `CreateAudioBackend()` factory in `AudioBackend.h`

```cpp
class IrrKlangBackend : public AudioBackend
{
    bool Init(const AudioBackendConfig& config) override { /* ... */ }
    bool PlaySound(const SoundBuffer& buffer, SoundInstance& instance) override { /* ... */ }
    // ... implement other methods
};

Ref<AudioBackend> CreateAudioBackend()
{
    // return CreateRef<MiniaudioBackend>();  // Old
    return CreateRef<IrrKlangBackend>();      // New
}
```

## Known Limitations

- **Callback thread**: `OnSoundFinished` callback currently runs on audio thread (should be queued to main thread)
- **Resource management**: Stopped sounds keep their backend handles until reused (minor memory overhead)
- **Format detection**: File format determined by extension only

## Build Configuration

Added to `premake5.lua`:

```lua
IncludeDir["miniaudio"] = "Hominem/vendor/miniaudio"

includedirs {
    "%{IncludeDir.miniaudio}"
}
```

miniaudio is a **single-header library** with no linking required. The implementation is compiled directly into `MiniaudioBackend.cpp`.

## Troubleshooting

### "Failed to initialize audio backend"
- Check audio device is available
- Verify no other application has exclusive access
- Try different sample rate (44100 or 48000)

### "Failed to load sound file"
- Verify file path is correct
- Check file format is supported
- Ensure file is not corrupted

### "No sound output"
- Check master volume: `audioSystem.SetMasterVolume(1.0f)`
- Verify sound is playing: `audioSystem.GetActiveSoundCount() > 0`
- Check system audio isn't muted

### "Clicking/Popping sounds"
- Increase buffer size: `config.BufferSize = 2048`
- Reduce active sound count
- Check CPU usage isn't maxed out
