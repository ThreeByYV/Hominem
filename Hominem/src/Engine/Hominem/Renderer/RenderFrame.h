#pragma once

#include <vector>
#include <span>
#include <string>
#include <memory>
#include <atomic>
#include <glm/glm.hpp>

#include "Hominem/Renderer/Texture.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Renderer/Frustum.h"
#include "Hominem/Renderer/FrameArena.h"

namespace Hominem {

	struct BakedEnvMap
	{
		std::atomic<bool> ready { false };
		Ref<TextureCube>  map;
	};

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

	enum class LightType : uint32_t { Point = 0, Spot = 1 };

	struct Light
	{
		glm::vec3 Position   = { 0.f, 1.f, 0.f };
		glm::vec3 Color      = { 1.f, 1.f, 1.f };
		glm::vec3 Direction  = { 0.f,-1.f, 0.f }; // Spot only
		float     Intensity  = 5.0f;
		float     Radius     = 3.0f;               // world-space range
		float     InnerAngle = 25.f;               // Spot only, degrees
		float     OuterAngle = 35.f;               // Spot only, degrees
		LightType Type       = LightType::Point;
	};

	struct DirectionalLight
	{
		glm::vec3 Direction        = glm::normalize(glm::vec3(0.5f, -1.f, 0.4f));
		glm::vec3 Color            = glm::vec3(1.f, 0.93f, 0.78f);
		glm::vec3 AmbientColor     = glm::vec3(0.4f, 0.45f, 0.6f); // cool blue-grey fill
		float     AmbientIntensity = 0.35f;
		float     DiffuseIntensity = 2.0f;
	};

	struct StaticMeshDraw
	{
		Ref<StaticMesh> mesh;
		glm::mat4       transform { 1.f };
		uint64_t        sortKey   = 0;    // (shaderID << 32 | meshID) — set at submit time
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
		glm::vec4        clearColor { 0.1f, 0.1f, 0.1f, 1.f };
		DirectionalLight light;

		bool  bloomEnabled       = false;
		bool  toneMappingEnabled = false;
		float bloomStrength      = 1.0f;
		float bloomThreshold     = 0.8f;
		float renderScale        = 1.0f; // 0.25–1.0, scales all internal FBOs

		glm::mat4 viewProjection2D {};
		glm::mat4 viewProjection3D {};
		glm::mat4 view3D           {};   // separate view matrix — needed by Forward+ light culling
		glm::mat4 proj3D           {};   // separate projection matrix — needed for tile frustum extraction
		glm::vec3 cameraWorldPos   {};
		Frustum   frustum3D        {};

		uint32_t  viewportWidth  = 0;
		uint32_t  viewportHeight = 0;

		bool debugLights = false;

		// Environment mapping — filled by Scene::BuildRenderFrame, consumed by Renderer3D
		Ref<TextureCube> envMap;
		float            envMapIntensity = 0.f;
		float            eta             = 0.667f;
		float            fresnelPower    = 5.f;

		bool                           bakeEnvMap    = false;
		glm::vec3                      bakeCapPos    = {};
		uint32_t                       bakeResolution = 512;
		std::shared_ptr<BakedEnvMap>   bakedEnvMapOut;  // render thread writes result here

		std::vector<QuadDraw>       quads;
		std::vector<TextDraw>       texts;
		std::vector<MeshDraw>       meshes;
		std::vector<StaticMeshDraw> staticMeshes;
		std::vector<Light>          lights;

		// Arena backing — set by RenderThread, not owned by this frame.
		FrameArena* arena    = nullptr;
		uint8_t     arenaIdx = 0;

		// Allocate count bone matrices from the arena and return a span.
		std::span<glm::mat4> AllocBones(size_t count)
		{
			if (!arena || count == 0) return {};
			auto* ptr = arena->Alloc<glm::mat4>(count);
			return { ptr, count };
		}
	};

}
