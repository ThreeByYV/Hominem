#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/UI/UIRoot.h"

namespace Hominem {

	class Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer() = default;

		virtual void OnAttach() {}
		virtual void OnDetach() {}

		virtual void OnUpdate(Timestep ts) {}

		virtual void OnImGuiRender() {}

		virtual void OnEvent(Event& event) {}

		void QueueTransition(std::unique_ptr<Layer> toLayer) const;

		Scene*  GetScene()  const { return m_Scene.get(); }
		UIRoot* GetUIRoot() const { return m_UI.get(); }

		/// Application pushes the window size in before OnAttach and on every resize,
		/// so layers never reach for the Application singleton to find it.
		void SetViewportSize(uint32_t width, uint32_t height);

		inline const std::string& GetName() const { return m_DebugName; }

		template<std::derived_from<Layer> T, typename...Args>
		void TransitionTo(Args&&... args)
		{
			QueueTransition(std::move(std::make_unique<T>(std::forward<Args>(args)...)));
		}

	protected:
		uint32_t GetViewportWidth()  const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }
		float    GetViewportAspect() const
		{
			return m_ViewportHeight > 0 ? (float)m_ViewportWidth / (float)m_ViewportHeight : 1.f;
		}

		uint32_t m_ViewportWidth  = 0;
		uint32_t m_ViewportHeight = 0;

		std::string m_DebugName;
		Ref<Scene>  m_Scene;
		Ref<UIRoot> m_UI;
	};
}
