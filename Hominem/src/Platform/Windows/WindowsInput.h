#pragma once

#include "Hominem/Core/Input.h"

namespace Hominem {

	class WindowsInput : public Input
	{
	protected:
		  bool IsKeyPressedImpl(int keycode) override;

		  bool IsMouseButtonPressedImpl(int button) override;
		  float GetMouseXImpl() override;
		  float GetMouseYImpl() override;
		  std::pair<float, float> GetMousePositionImpl() override;
	};

}


