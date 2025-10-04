#pragma once

#include "Layer.h"

namespace Hominem {

	class LayerStack
	{
	public:
		LayerStack();
		~LayerStack() = default;

		void PushLayer(std::unique_ptr<Layer> layer);      
		void PushOverlay(std::unique_ptr<Layer> overlay); 
		void PopLayer(Layer* layer);
		void PopOverlayer(Layer* overlay);


		//todo add reverse iterator, since will need to propagate events in a reverse fashion
		std::vector<std::unique_ptr<Layer>>::iterator begin() { return m_Layers.begin(); }
		std::vector<std::unique_ptr<Layer>>::iterator end() { return m_Layers.end(); }
	private:
		std::vector<std::unique_ptr<Layer>> m_Layers;
		unsigned int m_LayerInsertIndex = 0;
	};


}

