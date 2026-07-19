#pragma once

#include "Hominem/Core/Timestep.h"

#include <glm/glm.hpp>

namespace Hominem {

	struct RenderFrame;
	class UIRoot;

	/// A retained screen-space UI node. Coordinates are in overlay space: Y spans
	/// -1 (bottom) to +1 (top), X spans -aspect to +aspect.
	class Widget
	{
	public:
		virtual ~Widget() = default;

		virtual void OnCreate() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual void OnBuildRenderFrame(RenderFrame& frame) {}

		virtual bool ContainsPoint(glm::vec2 overlayPos) const
		{
			const glm::vec2 half = Size * 0.5f;
			return overlayPos.x >= Position.x - half.x && overlayPos.x <= Position.x + half.x
			    && overlayPos.y >= Position.y - half.y && overlayPos.y <= Position.y + half.y;
		}

		virtual void OnPointerEnter() {}
		virtual void OnPointerExit()  {}
		virtual bool OnPointerDown()  { return false; }

		glm::vec2 Position { 0.f };
		glm::vec2 Size     { 0.f };
		bool      Visible  = true;

		UIRoot* GetUIRoot() const { return m_UIRoot; }

	protected:
		UIRoot* m_UIRoot = nullptr;
		friend class UIRoot;
	};

}
