#pragma once

#include "Hominem/Core/Timestep.h"
#include "Hominem/Core/Tree.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Hominem {

	struct RenderFrame;
	class Scene;
	class Actor;

	/// Which actor hangs under which. World transforms are composed down it each frame.
	using SceneGraph = Tree<Actor*>;

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

		// Local to the parent in the scene graph (Scene::SetParent), or world for a root.
		glm::vec3 Position{ 0.f };
		glm::vec3 Scale{ 1.f };
		glm::vec3 Rotation{ 0.f }; // Euler angles in radians (XYZ)

		/// Local transform, relative to the parent.
		virtual glm::mat4 GetTransform() const
		{
			glm::mat4 rot = glm::toMat4(glm::quat(Rotation));
			return glm::translate(glm::mat4(1.f), Position)
				* rot
				* glm::scale(glm::mat4(1.f), Scale);
		}

		/// Written by the scene graph each frame, after OnUpdate and before drawing.
		const glm::mat4& GetWorldTransform() const { return m_WorldTransform; }

		glm::vec3 GetWorldPosition() const { return glm::vec3(GetWorldTransform()[3]); }

		/// Sets Position/Scale directly and Rotation from degrees, in one call.
		void SetTransformDeg(const glm::vec3& pos, const glm::vec3& rotDeg, const glm::vec3& scale)
		{
			Position = pos;
			Rotation = glm::radians(rotDeg);
			Scale    = scale;
		}

		glm::vec3 GetRotationDeg() const { return glm::degrees(Rotation); }

		Scene* GetScene() const { return m_Scene; }

	protected:
		Scene* m_Scene = nullptr;
		friend class Scene;

	private:
		SceneGraph::NodeId m_Node           = SceneGraph::Null;
		glm::mat4          m_WorldTransform { 1.f };
	};

}
