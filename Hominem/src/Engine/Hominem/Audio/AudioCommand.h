#pragma once

#include <variant>
#include "SoundInstance.h"

namespace Hominem {

/// Commands sent from the main thread to the audio thread.

struct PlaySoundCmd
{
    SoundHandle      Handle;
    Ref<SoundBuffer> Buffer;
    float Volume      = 1.0f;
    float Pitch       = 1.0f;
    float Pan         = 0.0f;
    bool  Loop        = false;
    float StartOffset = 0.0f; // seconds into the sound to start playback from
};

struct StopSoundCmd       { SoundHandle Handle; };
struct PauseSoundCmd      { SoundHandle Handle; };
struct ResumeSoundCmd     { SoundHandle Handle; };
struct SetVolumeCmd       { SoundHandle Handle; float Volume; };
struct SetPitchCmd        { SoundHandle Handle; float Pitch; };
struct SetPanCmd          { SoundHandle Handle; float Pan; };
struct SetMasterVolumeCmd { float Volume; };
struct StopAllCmd         {};
struct ShutdownCmd        {};

using AudioCommand = std::variant<
    PlaySoundCmd,
    StopSoundCmd,
    PauseSoundCmd,
    ResumeSoundCmd,
    SetVolumeCmd,
    SetPitchCmd,
    SetPanCmd,
    SetMasterVolumeCmd,
    StopAllCmd,
    ShutdownCmd
>;

}
