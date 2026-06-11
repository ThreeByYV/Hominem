#pragma once

#include <string>

namespace Hominem {

	class Cutscene;
	struct CutsceneContext;

	/**
	 * @brief ImGui authoring panel for a Cutscene: transport, a draggable timeline,
	 *        per-cue property editing, add/remove, and save-to-JSON.
	 *
	 * Drives the cutscene clock directly while open. When paused it re-previews each
	 * frame so edits are reflected live without firing event/audio side effects.
	 */
	class CutsceneEditor
	{
	public:
		/// @brief Draw the editor window and apply any interactions to @p cs.
		void OnImGui(Cutscene& cs, CutsceneContext& ctx);

		bool        Open     = true;
		std::string SavePath; ///< Where the Save button writes.

	private:
		float m_PixelsPerSecond = 80.f;
	};

}
