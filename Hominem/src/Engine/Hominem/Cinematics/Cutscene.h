#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Cinematics/Cue.h"

#include <vector>
#include <functional>
#include <utility>

namespace Hominem {

	/**
	 * @brief A flat list of cues plus a playback clock.
	 *
	 * Deliberately simpler than a lanes-contain-blocks model: there is no "lane"
	 * object — a cue *is* the block. The whole cutscene is driven by one clock, so
	 * playback is a pure function of time (the basis for skip / pause / scrub).
	 */
	class Cutscene
	{
	public:
		/// @brief Construct a cue in place and append it. Returns a reference for
		///        fluent configuration: cs.Add<SpriteCue>(...).At(1.5f).For(3.0f);
		template<typename T, typename... Args>
		T& Add(Args&&... args)
		{
			auto cue = CreateScope<T>(std::forward<Args>(args)...);
			T& ref = *cue;
			m_Cues.push_back(std::move(cue));
			return ref;
		}

		/// @brief Append an already-constructed cue (used by the JSON loader).
		Cue& AddCue(Scope<Cue> cue)
		{
			Cue& ref = *cue;
			m_Cues.push_back(std::move(cue));
			return ref;
		}

		/// @brief Remove all cues.
		void Clear() { m_Cues.clear(); m_Time = 0.f; m_Playing = false; m_Finished = false; }

		/// @brief Start playback from the beginning.
		void Play();

		/// @brief Continue playback from the current time without rewinding.
		void Resume() { if (!m_Finished) m_Playing = true; }

		/// @brief Halt playback without firing the finished callback.
		void Stop() { m_Playing = false; }

		bool  IsPlaying()  const { return m_Playing; }
		bool  IsFinished() const { return m_Finished; }
		float Time()       const { return m_Time; }

		/// @brief Total length: the latest EndTime across all cues.
		float Duration() const;

		/// @brief Advance the clock and run cue lifecycle hooks (main-thread sim phase).
		void OnUpdate(Timestep ts, CutsceneContext& ctx);

		/// @brief Collect draw commands from currently-active cues (render-frame build phase).
		void BuildRenderFrame(RenderFrame& frame) const;

		/**
		 * @brief Set the clock to @p t and recompute which cues are active, for a
		 *        rendered preview — WITHOUT firing OnEnter/OnExit side effects.
		 *
		 * This is what the editor uses while scrubbing: it updates the visual state
		 * (fades, sprites) but never re-triggers events or audio. Lifecycle flags are
		 * left consistent so a subsequent Resume() picks up cleanly.
		 */
		void PreviewAt(float t);

		// Editor access
		size_t CueCount()        const { return m_Cues.size(); }
		Cue&   GetCue(size_t i)        { return *m_Cues[i]; }
		const Cue& GetCue(size_t i) const { return *m_Cues[i]; }
		void   RemoveCue(size_t i);

		/// @brief Invoked once when the playhead passes the last cue's EndTime.
		void SetOnFinished(std::function<void()> cb) { m_OnFinished = std::move(cb); }

		/// @brief Call OnTick on all currently-active cues without advancing the clock.
		/// Used by the editor so camera/scene cues stay in sync while scrubbing.
		void TickCues(CutsceneContext& ctx);

	private:
		std::vector<Scope<Cue>> m_Cues;
		float m_Time     = 0.f;
		bool  m_Playing  = false;
		bool  m_Finished = false;
		std::function<void()> m_OnFinished;
	};

}
