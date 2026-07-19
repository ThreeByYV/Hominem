#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/UI/Widget.h"

#include <memory>
#include <optional>
#include <vector>

namespace Hominem {

	struct RenderFrame;

	/// Owns a retained widget tree and drives it: layout space, update, draw
	/// submission and pointer routing. The screen-space counterpart to Scene.
	class UIRoot : public RefCounted
	{
	public:
		template<typename T, typename... Args>
		T& AddWidget(Args&&... args)
		{
			auto widget = std::make_unique<T>(std::forward<Args>(args)...);
			T&   ref    = *widget;
			ref.m_UIRoot = this;
			m_Widgets.push_back(std::move(widget));
			ref.OnCreate();
			return ref;
		}

		void OnUpdate(Timestep ts);
		void BuildRenderFrame(RenderFrame& frame);

		void OnViewportResize(uint32_t width, uint32_t height);
		void OnPointerMove(float screenX, float screenY);
		bool OnPointerDown();

		glm::vec2 ScreenToOverlay(float screenX, float screenY) const;

		float GetAspect() const { return m_Aspect; }

		/// Set on UI-only layers that have no Scene to establish the frame's clear colour.
		std::optional<glm::vec4> ClearColor;

		static constexpr float kOverlayHeight = 2.0f;

	private:
		std::vector<std::unique_ptr<Widget>> m_Widgets;

		uint32_t m_ViewportWidth  = 0;
		uint32_t m_ViewportHeight = 0;
		float    m_Aspect         = 1.f;

		Widget*   m_HoveredWidget = nullptr;
		glm::vec2 m_PointerOverlayPos { 0.f };
		bool      m_PointerWasDown = false;
	};

}
