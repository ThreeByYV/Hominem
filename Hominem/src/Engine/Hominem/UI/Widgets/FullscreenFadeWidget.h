#pragma once

#include "Hominem/UI/Widgets/QuadWidget.h"

namespace Hominem {

	/// A solid colour covering the whole overlay, for transitions and letterboxing.
	class FullscreenFadeWidget : public QuadWidget
	{
	public:
		FullscreenFadeWidget() { StretchToViewport = true; }

		explicit FullscreenFadeWidget(glm::vec4 color)
		{
			Color             = color;
			StretchToViewport = true;
		}
	};

}
