#pragma once

#include "Hominem/Core/Timestep.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Hominem {

	class Scene;

	class Actor
	{
	public:
		virtual ~Actor() = default;

		/// Called once after the actor is added to the scene and m_Scene is set.
		virtual void OnCreate()  {}

		/// Called every frame before drawing.
		virtual void OnUpdate(Timestep ts) {}

		/// Called every frame during the Renderer2D pass (sprites, text, etc.).
		virtual void OnDraw2D() {}

		/// Called every frame during the Renderer3D pass (skinned meshes, etc.).
		virtual void OnDraw3D() {}

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
