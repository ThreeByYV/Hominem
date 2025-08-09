#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Events/KeyEvent.h"

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
			//HMN_CORE_INFO("ExampleLayer::Update");
			
			if (Input::IsKeyPressed(HMN_KEY_TAB))
			{
				HMN_CORE_TRACE("Tab key is pressed (poll)");
			}
				
		}

		void OnEvent(Hominem::Event& event) override
		{
			//HMN_CORE_TRACE("{0}", event.ToString());
			if (event.GetEventType() == EventType::KeyPressed)
			{
				KeyPressedEvent& e = (KeyPressedEvent&)event;
				if (e.GetKeyCode() == HMN_KEY_TAB)
				{
					HMN_CORE_TRACE("Tab key is pressed (event)");
				}

				HMN_CORE_TRACE("{0}", (char)e.GetKeyCode());
			}
		}

	};
}