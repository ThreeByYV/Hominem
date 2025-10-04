#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"

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

		inline const std::string& GetName() const { return m_DebugName; }

		template<std::derived_from<Layer> T, typename...Args>
		void TransitionTo(Args&&... args)
		{
			QueueTransition(std::move(std::make_unique<T>(std::forward<Args>(args)...)));
		}

	protected:
		std::string m_DebugName;

	private:
		void QueueTransition(std::unique_ptr<Layer> layer);
	};
} 
