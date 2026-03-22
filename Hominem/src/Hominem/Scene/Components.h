#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "SceneCamera.h"
#include "Hominem/Mesh/SkinnedMesh.h"
#include "Hominem/Audio/SoundInstance.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/Font.h"

namespace Hominem {

	class Rigidbody;
	class BoxCollider;
}

namespace Hominem {

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag)
		{
		};

	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };  // Euler angles (radians)
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation)
		{
		}

		// Build transform matrix from components
		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));
			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}

		operator glm::mat4() { return GetTransform(); }
		operator const glm::mat4() const { return GetTransform(); }
	};

	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Ref<Texture2D> Texture = nullptr;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color)
		{
		};
		SpriteRendererComponent(const Ref<Texture2D>& texture, const glm::vec4& tint = glm::vec4(1.0f))
			: Texture(texture), Color(tint)
		{
		};

		operator glm::vec4& () { return Color; }
		operator const glm::vec4& () const { return Color; }
	};

	struct TextRendererComponent
	{
		std::string Text;
		Ref<Font> Font;
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

		TextRendererComponent() = default;
		TextRendererComponent(const TextRendererComponent&) = default;
		TextRendererComponent(const std::string& text, const Ref<Hominem::Font>& font = nullptr, const glm::vec4& color = glm::vec4(1.0f))
			: Text(text), Font(font), Color(color)
		{
		};
	};

	struct CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;

	};

	// Models
	struct SkinnedMeshComponent
	{
		Ref<SkinnedMesh> Mesh;
		std::string FilePath;

		SkinnedMeshComponent() = default;
		SkinnedMeshComponent(const SkinnedMeshComponent&) = default;
		SkinnedMeshComponent(const Ref<SkinnedMesh>& mesh, const std::string& filepath = "")
			: Mesh(mesh), FilePath(filepath)
		{
		};
	};

	struct AnimationComponent
	{
		float AnimationTime = 0.0f;
		float AnimationSpeed = 1.0f;
		bool Playing = true;

		// Blending support
		bool UseBlending = false;
		uint32_t StartAnimIndex = 0;
		uint32_t EndAnimIndex = 0;
		float BlendFactor = 0.0f;      // 0.0 = fully startAnim, 1.0 = fully endAnim
		float BlendSpeed = 5.0f;       // How fast to blend between animations
		uint32_t TargetAnimIndex = 0;  // Animation we're blending towards

		// Root motion control
		bool DisableRootMotion = true; // Ignore translation in animation (for 2.5D games)

		AnimationComponent() = default;
		AnimationComponent(const AnimationComponent&) = default;
		AnimationComponent(float speed, bool playing = true)
			: AnimationSpeed(speed), Playing(playing)
		{
		};
	};

	// Audio
	struct AudioSourceComponent
	{
		SoundBufferHandle Buffer = InvalidSoundBuffer;
		SoundHandle ActiveHandle = InvalidSound;

		// Desired playback state
		bool ShouldPlay = false;
		bool Loop = false;
		float Volume = 1.0f;
		float Pitch = 1.0f;
		float Pan = 0.0f;

		// Internal tracking
		bool IsPlaying = false;
		bool PropertiesDirty = false;

		AudioSourceComponent() = default;
		AudioSourceComponent(const AudioSourceComponent&) = default;
		AudioSourceComponent(SoundBufferHandle buffer, float volume = 1.0f, bool loop = false)
			: Buffer(buffer), Volume(volume), Loop(loop)
		{
		};
	};

	struct AudioListenerComponent
	{
		bool Active = true;

		AudioListenerComponent() = default;
		AudioListenerComponent(const AudioListenerComponent&) = default;
	};

	// Physics 
	struct RigidbodyComponent
	{
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type = BodyType::Dynamic;

		float Mass            = 1.0f;
		float LinearDamping   = 0.0f;
		float AngularDamping  = 0.05f;

		// 2.5D: lock rotation so character stays upright on 2D plane
		bool FixedRotation    = true;

		bool GravityEnabled   = true;

		Ref<Rigidbody> RuntimeBody = nullptr;

		RigidbodyComponent() = default;
		RigidbodyComponent(const RigidbodyComponent&) = default;
	};

	struct BoxColliderComponent
	{
		// X/Y used for Box2D, Z ignored at runtime
		glm::vec2 HalfExtents = { 0.5f, 0.5f };
		glm::vec2 Offset      = { 0.0f, 0.0f };

		float StaticFriction  = 0.5f;
		float DynamicFriction = 0.5f;
		float Restitution     = 0.0f;

		bool IsTrigger = false;

		Ref<BoxCollider> RuntimeCollider = nullptr;

		BoxColliderComponent() = default;
		BoxColliderComponent(const BoxColliderComponent&) = default;
	};

}