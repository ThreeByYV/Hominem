#pragma once

#include <variant>

#include "SoundInstance.h"

namespace Hominem {

	/// @brief Commands sent from main thread to audio thread.

	struct PlaySoundCmd
	{
		SoundHandle Handle;
		SoundBufferHandle BufferHandle;
		float Volume = 1.0f;
		float Pitch = 1.0f;
		float Pan = 0.0f;
		bool Loop = false;
	};

	struct StopSoundCmd { SoundHandle Handle; };
	struct PauseSoundCmd { SoundHandle Handle; };
	struct ResumeSoundCmd { SoundHandle Handle; };
	struct SetVolumeCmd { SoundHandle Handle; float Volume; };
	struct SetPitchCmd { SoundHandle Handle; float Pitch; };
	struct SetPanCmd { SoundHandle Handle; float Pan; };
	struct SetMasterVolumeCmd { float Volume; };
	struct StopAllCmd {};
	struct ShutdownCmd {};

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
