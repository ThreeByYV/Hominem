#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Hominem/Scene/Scene.h"

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

		/// Calls m_Scene->BuildRenderFrame(frame) if a scene is set.
		/// Override to add extra draws; call Layer::OnBuildRenderFrame(frame) first.
		virtual void OnBuildRenderFrame(RenderFrame& frame);

		void QueueTransition(std::unique_ptr<Layer> toLayer) const;

		inline const std::string& GetName() const { return m_DebugName; }

		template<std::derived_from<Layer> T, typename...Args>
		void TransitionTo(Args&&... args)
		{
			QueueTransition(std::move(std::make_unique<T>(std::forward<Args>(args)...)));
		}

	protected:
		/// For scene-less layers (loading screens, transition layers): sets frame.viewportWidth/Height
		/// from the window and frame.clearColor, since there's no Scene to do that automatically.
		static void SetFullscreenClear(RenderFrame& frame, const glm::vec4& clearColor);

		std::string m_DebugName;
		Ref<Scene>  m_Scene;
	};
}
