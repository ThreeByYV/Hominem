#include "hmnpch.h"
#include "Hominem/Cinematics/Cutscene.h"

#include <algorithm>

namespace Hominem {

	void Cutscene::Play()
	{
		m_Time     = 0.f;
		m_Playing  = true;
		m_Finished = false;

		// Re-arm every cue so a replay fires the full lifecycle again.
		for (auto& c : m_Cues)
		{
			c->m_Entered = false;
			c->m_Exited  = false;
			c->m_Active  = false;
			c->m_LocalT  = 0.f;
		}
	}

	float Cutscene::Duration() const
	{
		float d = 0.f;
		for (const auto& c : m_Cues)
			d = std::max(d, c->EndTime());
		return d;
	}

	void Cutscene::OnUpdate(Timestep ts, CutsceneContext& ctx)
	{
		if (!m_Playing)
			return;

		m_Time += ts.GetSeconds();

		for (auto& c : m_Cues)
		{
			const bool inWindow = m_Time >= c->StartTime && m_Time <= c->EndTime();

			// Instant cues (Duration 0) have an empty window, so trigger them the
			// moment the playhead reaches StartTime instead.
			const bool reachedInstant = c->Duration <= 0.f && m_Time >= c->StartTime;

			if ((inWindow || reachedInstant) && !c->m_Entered)
			{
				c->OnEnter(ctx);
				c->m_Entered = true;
				c->m_Active  = true;
			}

			if (c->m_Active && inWindow)
			{
				c->m_LocalT = m_Time - c->StartTime;
				c->OnTick(ctx, c->m_LocalT);
			}

			if (c->m_Entered && !c->m_Exited && m_Time > c->EndTime())
			{
				c->OnExit(ctx);
				c->m_Exited = true;
				c->m_Active = false;
			}
		}

		if (!m_Finished && m_Time >= Duration())
		{
			m_Finished = true;
			m_Playing  = false;
			if (m_OnFinished)
				m_OnFinished();
		}
	}

	void Cutscene::BuildRenderFrame(RenderFrame& frame) const
	{
		for (auto& c : m_Cues)
			if (c->m_Active)
				c->OnBuildRenderFrame(frame, c->m_LocalT);
	}

	void Cutscene::PreviewAt(float t)
	{
		m_Time = t;

		for (auto& c : m_Cues)
		{
			const bool before = t < c->StartTime;
			const bool after  = t > c->EndTime();

			c->m_Active  = !before && !after;
			c->m_LocalT  = t - c->StartTime;
			c->m_Entered = !before; // treat as entered once the playhead is at/after start
			c->m_Exited  = after;   // treat as exited once past the end
		}
	}

	void Cutscene::TickCues(CutsceneContext& ctx)
	{
		for (auto& c : m_Cues)
			if (c->m_Active)
				c->OnTick(ctx, c->m_LocalT);
	}

	void Cutscene::RemoveCue(size_t i)
	{
		if (i < m_Cues.size())
			m_Cues.erase(m_Cues.begin() + i);
	}

}
