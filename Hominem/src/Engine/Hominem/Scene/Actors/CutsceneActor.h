#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Cinematics/Cutscene.h"
#include "Hominem/Renderer/RenderFrame.h"

namespace Hominem {

	/// Draws a Cutscene's cues. The Cutscene is owned elsewhere and outlives the actor.
	class CutsceneActor : public Actor
	{
	public:
		explicit CutsceneActor(Cutscene& cutscene) : m_Cutscene(cutscene) {}

		void OnBuildRenderFrame(RenderFrame& frame) override
		{
			m_Cutscene.BuildRenderFrame(frame);
		}

	private:
		Cutscene& m_Cutscene;
	};

}
