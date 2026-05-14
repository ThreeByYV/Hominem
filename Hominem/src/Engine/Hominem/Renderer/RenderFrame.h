#pragma once

#include <vector>
#include <span>
#include <string>
#include <glm/glm.hpp>

#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Renderer/Frustum.h"
#include "Hominem/Renderer/FrameArena.h"

namespace Hominem {

	struct QuadDraw
	{
		glm::mat4      transform { 1.f };
		glm::vec4      color     { 1.f };
		glm::vec2      uvMin     { 0.f, 0.f };
		glm::vec2      uvMax     { 1.f, 1.f };
		Ref<Texture2D> texture;
	};

	struct TextDraw
	{
		std::string text;
		Ref<Font>   font;
		glm::mat4   transform { 1.f };
		glm::vec4   color     { 1.f };
	};

	/**
	 * Skinned mesh draw. bones is a span into the frame arena — no heap alloc.
	 * Allocate with: frame.AllocBones(count), write transforms, store span here.
	 */
	struct MeshDraw
	{
		Ref<SkinnedMesh>        mesh;
		glm::mat4               transform { 1.f };
		std::span<glm::mat4>    bones;     // points into FrameArena, zero-cost to copy
	};

	struct StaticMeshDraw
	{
		Ref<StaticMesh> mesh;
		glm::mat4       transform { 1.f };
	};

	/**
	 * Plain-data snapshot of everything one frame needs to render.
	 * Built on the main thread, consumed by the render thread.
	 *
	 * arena / arenaIdx are set by RenderThread before OnBuildRenderFrame runs.
	 * Use AllocBones() to bump-allocate bone matrices into the arena.
	 */
	struct RenderFrame
	{
		glm::vec4 clearColor       { 0.1f, 0.1f, 0.1f, 1.f };

		glm::mat4 viewProjection2D {};
		glm::mat4 viewProjection3D {};
		glm::vec3 cameraWorldPos   {};
		Frustum   frustum3D        {};

		uint32_t  viewportWidth  = 0;
		uint32_t  viewportHeight = 0;

		std::vector<QuadDraw>       quads;
		std::vector<TextDraw>       texts;
		std::vector<MeshDraw>       meshes;
		std::vector<StaticMeshDraw> staticMeshes;

		// Arena backing — set by RenderThread, not owned by this frame.
		FrameArena* arena    = nullptr;
		uint8_t     arenaIdx = 0;

		// Allocate count bone matrices from the arena and return a span.
		std::span<glm::mat4> AllocBones(size_t count)
		{
			if (!arena || count == 0) return {};
			glm::mat4* ptr = arena->Alloc<glm::mat4>(count);
			return { ptr, count };
		}
	};

}
