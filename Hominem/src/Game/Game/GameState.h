#pragma once

#include <string>

namespace Hominem {

	struct GameState
	{
		// Progression
		int   CurrentLevel     = 1;
		int   CheckpointIndex  = 0;
		float PlayTime         = 0.0f;

		// Player
		bool  PlayerAlive      = true;
		int   Deaths           = 0;

		// Level
		std::string CurrentLevelScript   = "";
		std::string LastCheckpointScript = "";

		static GameState& Get()
		{
			static GameState s_Instance;
			return s_Instance;
		}

	private:
		GameState() = default;
	};

}
