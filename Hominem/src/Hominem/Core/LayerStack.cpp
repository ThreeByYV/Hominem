#include "hmnpch.h"
#include "LayerStack.h"


//Layers are owned by the Application and only truly get removed when the Application dies
namespace Hominem {

	LayerStack::LayerStack()
	{
	}

	void LayerStack::PushLayer(std::unique_ptr<Layer> layer)
	{
		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, std::move(layer));
		m_LayerInsertIndex++;
	}

	void LayerStack::PushOverlay(std::unique_ptr<Layer> overlay)
	{
		m_Layers.emplace_back(std::move(overlay));
	}

	void LayerStack::PopLayer(Layer* layer)
	{
		auto it = std::find_if(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex,
			[layer](const std::unique_ptr<Layer>& p) {
				return p.get() == layer;  // Must compare raw pointers since unique ptr doesn't have equality operator
			});

		if (it != m_Layers.begin() + m_LayerInsertIndex)
		{
			(*it)->OnDetach();  
			m_Layers.erase(it);
			m_LayerInsertIndex--;
		}
	}

	//Overlays will always be rendered last, always behind all Layers
	void LayerStack::PopOverlayer(Layer* overlay)
	{
		auto it = std::find_if(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(),
			[overlay](const std::unique_ptr<Layer>& p) {
				return p.get() == overlay;
			});

		if (it != m_Layers.end())
		{
			(*it)->OnDetach();  
			m_Layers.erase(it);
		}
	}
};