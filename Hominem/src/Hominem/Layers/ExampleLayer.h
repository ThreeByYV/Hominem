#pragma once

#include "Hominem/Core/Layer.h"

namespace Hominem {

	class ExampleLayer : public Layer
	{
	public:
		ExampleLayer()
			: Layer("Example")
		{
		}

		void OnUpdate() override
		{
			/*HMN_CORE_INFO("ExampleLayer::Update");*/
		}

		void OnEvent(Hominem::Event& event) override
		{
			HMN_CORE_TRACE("{0}", event.ToString());
		}

	};
}