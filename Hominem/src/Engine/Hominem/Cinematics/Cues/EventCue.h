#pragma once

#include "Hominem/Cinematics/Cue.h"

#include <functional>
#include <utility>

namespace Hominem {

	/// @brief Instant cue (Duration 0) that fires a callback once when the playhead
	///        reaches StartTime — used to trigger gameplay events from the timeline.
	class EventCue : public Cue
	{
	public:
		explicit EventCue(std::function<void()> fn)
			: m_Fn(std::move(fn))
		{
		}

		const char* TypeName() const override { return "event"; }

		void OnEnter(CutsceneContext&) override
		{
			if (m_Fn)
				m_Fn();
		}

	private:
		std::function<void()> m_Fn;
	};

}
