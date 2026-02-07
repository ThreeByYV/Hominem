#pragma once

#include <glm/glm.hpp>
#include "SceneCamera.h"
#include "Hominem/Mesh/SkinnedMesh.h"
#include "Hominem/Audio/SoundInstance.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/Font.h"

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
		glm::mat4 Transform{ 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::mat4& transform)
			: Transform(transform)
		{
		};

		operator glm::mat4& () { return Transform; }
		operator const glm::mat4& () const { return Transform;  }
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

		AnimationComponent() = default;
		AnimationComponent(const AnimationComponent&) = default;
		AnimationComponent(float speed, bool playing = true)
			: AnimationSpeed(speed), Playing(playing)
		{
		};
	};

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
}