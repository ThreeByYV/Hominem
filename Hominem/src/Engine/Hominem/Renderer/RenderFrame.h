#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>

#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Renderer/Frustum.h"

namespace Hominem {

	/**
	 * All 2D sprite/quad data for one frame — no GL types, safe to build on any thread.
	 * UV defaults to full texture. Null texture = white.
	 */
	struct QuadDraw
	{
		glm::mat4      transform { 1.f };
		glm::vec4      color     { 1.f };
		glm::vec2      uvMin     { 0.f, 0.f };
		glm::vec2      uvMax     { 1.f, 1.f };
		Ref<Texture2D> texture;
	};

	/**
	 * Text draw command — render thread calls Renderer2D::DrawString with these fields.
	 */
	struct TextDraw
	{
		std::string text;
		Ref<Font>   font;
		glm::mat4   transform { 1.f };
		glm::vec4   color     { 1.f };
	};

	/**
	 * Skinned mesh draw — render thread calls DispatchSkinning then DrawSkinnedMesh.
	 * Ref<SkinnedMesh> keeps the mesh alive even if the owning actor is destroyed
	 * between Submit() and ExecuteFrame() during a layer transition.
	 */
	struct MeshDraw
	{
		Ref<SkinnedMesh>       mesh;
		glm::mat4              transform { 1.f };
		std::vector<glm::mat4> bones;
	};

	/**
	 * Static mesh draw — frustum culling already applied on main thread so every
	 * entry here is guaranteed visible and needs a draw call.
	 */
	struct StaticMeshDraw
	{
		Ref<StaticMesh> mesh;
		glm::mat4       transform { 1.f };
	};

	/**
	 * Plain-data snapshot of everything one frame needs to render.
	 * Built on the main thread, consumed by the render thread — no GL calls during build.
	 *
	 */
	struct RenderFrame
	{
		glm::vec4 clearColor       { 0.1f, 0.1f, 0.1f, 1.f };

		glm::mat4 viewProjection2D {};
		glm::mat4 viewProjection3D {};
		glm::vec3 cameraWorldPos   {};
		Frustum   frustum3D        {};  // extracted from viewProjection3D — used for culling

		uint32_t  viewportWidth  = 0;
		uint32_t  viewportHeight = 0;

		std::vector<QuadDraw>       quads;
		std::vector<TextDraw>       texts;
		std::vector<MeshDraw>       meshes;
		std::vector<StaticMeshDraw> staticMeshes; // pre-culled by main thread
	};

}
