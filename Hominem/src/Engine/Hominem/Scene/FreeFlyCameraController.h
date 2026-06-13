#pragma once

#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Scene/Scene.h"

#include <glm/glm.hpp>

namespace Hominem {

	class FreeFlyCameraController
	{
	public:
		bool Enabled = false;

		/// Returns true (handled) if this consumed the key event.
		bool OnKeyPressed(KeyPressedEvent& e);

		/// No-op while disabled.
		void OnUpdate(Timestep ts, Scene& scene);

	private:
		float     m_Yaw     = 0.f;
		float     m_Pitch   = 0.f;
		bool      m_Looking = false; ///< LMB held - capturing mouse delta.
		glm::vec2 m_LastMouse{ 0.f, 0.f };
		float     m_Speed = 3.0f;
	};

}
