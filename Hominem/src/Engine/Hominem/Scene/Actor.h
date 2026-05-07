#pragma once

#include "Hominem/Core/Timestep.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Hominem {

	struct RenderFrame;
	class Scene;

	class Actor
	{
	public:
		virtual ~Actor() = default;

		virtual void OnCreate()  {}
		virtual void OnUpdate(Timestep ts) {}

		/// Push draw commands into the frame — no GL calls allowed here.
		virtual void OnBuildRenderFrame(RenderFrame& frame) {}

		/// Called when the actor is removed from the scene.
		virtual void OnDestroy() {}

		glm::vec3 Position{ 0.f };
		glm::vec3 Scale{ 1.f };
		glm::vec3 Rotation{ 0.f }; // Euler angles in radians (XYZ)

		glm::mat4 GetTransform() const
		{
			glm::mat4 rot = glm::toMat4(glm::quat(Rotation));
			return glm::translate(glm::mat4(1.f), Position)
				* rot
				* glm::scale(glm::mat4(1.f), Scale);
		}

		Scene* GetScene() const { return m_Scene; }

	protected:
		Scene* m_Scene = nullptr;
		friend class Scene;
	};

}
