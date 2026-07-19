#include "hmnpch.h"
#include "Layer.h"

#include "Application.h"

namespace Hominem {

	Layer::Layer(const std::string& debugName)
		: m_DebugName(debugName)
	{
	}

	void Layer::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;

		m_ViewportWidth  = width;
		m_ViewportHeight = height;

		if (m_Scene) m_Scene->OnViewportResize(width, height);
		if (m_UI)    m_UI->OnViewportResize(width, height);
	}

	void Layer::QueueTransition(std::unique_ptr<Layer> toLayer) const
	{
		Application::Get().QueueLayerTransition(this->GetName(), std::move(toLayer));
	}
}