#include "hmnpch.h"
#include "Layer.h"

#include "Application.h"

namespace Hominem {

	Layer::Layer(const std::string& debugName)
		: m_DebugName(debugName)
	{
	}

	void Layer::OnBuildRenderFrame(RenderFrame& frame)
	{
		if (m_Scene) m_Scene->BuildRenderFrame(frame);
	}

	void Layer::QueueTransition(std::unique_ptr<Layer> toLayer) const
	{
		Application::Get().QueueLayerTransition(this->GetName(), std::move(toLayer));
	}
}