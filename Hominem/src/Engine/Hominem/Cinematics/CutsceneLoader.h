#pragma once

#include "Hominem/Cinematics/Cutscene.h"

#include <string>

namespace Hominem {

	/// @brief Reads and writes a Cutscene as JSON — the on-disk format the editor
	///        saves to and the game loads from.
	class CutsceneLoader
	{
	public:
		/// @brief Clear @p cs and populate it from the JSON file. Returns false on error.
		static bool LoadFromFile(const std::string& path, Cutscene& cs);

		/// @brief Write @p cs to a JSON file (creating parent dirs). Returns false on error.
		static bool SaveToFile(const std::string& path, const Cutscene& cs);
	};

}
