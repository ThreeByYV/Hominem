#pragma once

#include "Hominem/Renderer/RenderFrame.h"

namespace Hominem {

	class Cutscene;
	class Scene;
	class SkinnedMesh;

	/// @brief Shared state a cue may reach into while it plays.
	/// Grows as new cue types arrive (AudioSystem*, ...).
	struct CutsceneContext
	{
		Cutscene*    cutscene   = nullptr;
		Scene*       scene      = nullptr;  ///< Set by the layer; gives cues access to the camera.
		SkinnedMesh* targetMesh = nullptr;  ///< Optional; camera cues read bone sockets from here.
	};

	/**
	 * @brief One timed block on a cutscene timeline.
	 *
	 * A cue means "starting at StartTime, for Duration seconds, do this job."
	 * A cue with Duration == 0 is an instant trigger (fires once, no span).
	 *
	 */
	class Cue
	{
	public:
		virtual ~Cue() = default;

		float StartTime = 0.f;
		float Duration  = 0.f;

		/// @brief Timeline position at which this cue stops.
		float EndTime() const { return StartTime + Duration; }

		/// @brief Stable identifier used for JSON serialization and editor labels.
		/// Concrete cues override this with their "type" string ("fade", "sprite", ...).
		virtual const char* TypeName() const { return "cue"; }

		/// @brief Fluent setter for StartTime, e.g. cs.Add<SpriteCue>(...).At(1.5f).
		Cue& At(float t)  { StartTime = t; return *this; }

		/// @brief Fluent setter for Duration, e.g. .For(3.0f).
		Cue& For(float d) { Duration  = d; return *this; }

		virtual void OnEnter(CutsceneContext&) {}
		virtual void OnTick (CutsceneContext&, float localT) {}
		virtual void OnExit (CutsceneContext&) {}

		/// @brief Push draw commands for this cue. No GL calls here (same rule as Actor).
		/// @param localT Seconds elapsed since the cue became active.
		virtual void OnBuildRenderFrame(RenderFrame&, float localT) {}

	private:
		bool  m_Entered = false;
		bool  m_Exited  = false;
		bool  m_Active  = false;
		float m_LocalT  = 0.f;

		friend class Cutscene;
	};

}
