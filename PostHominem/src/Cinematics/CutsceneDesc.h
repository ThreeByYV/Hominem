#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace Hominem { class Scene; class Layer; }

struct FireInstance
{
	glm::vec3 pos         {};
	glm::vec3 rotDeg      {};
	glm::vec2 size        { 10.f, 10.f };
	float     intensity   = 1.0f;
	float     scrollSpeed = 0.5f;
	float     seed        = 0.f;
};

struct SmokeInstance
{
	glm::vec3 pos         {};
	glm::vec3 rotDeg      {};
	glm::vec2 size        { 10.f, 10.f };
	float     opacity     = 0.5f;
	float     scrollSpeed = 0.15f;
	float     seed        = 0.f;
};

struct CutsceneDesc
{
	// --- Assets (virtual paths, e.g. "game://Textures/...") ---
	struct Assets
	{
		std::string setMesh;
		std::string skybox;
		std::string cutsceneJson;
	} assets;

	// --- Camera framing ---
	struct Camera
	{
		float     fov    = 90.f;
		glm::vec3 pos    = {};
		glm::vec3 target = { 1.f, 0.f, 0.f };
	} camera;

	// --- 3D set transform ---
	struct Set
	{
		glm::vec3 pos    = {};
		glm::vec3 scale  = { 1.f, 1.f, 1.f };
		glm::vec3 rotDeg = {};
	} set;

	// --- Directional light + sky ---
	struct Light
	{
		glm::vec3 dir         = { -0.2f, -1.f, -0.4f };
		glm::vec3 color       = { 1.f, 1.f, 1.f };
		float     ambient     = 0.3f;
		float     diffuse     = 1.0f;
		float     skyIntensity = 1.0f;
	} light;

	// --- Procedural effects ---
	std::vector<FireInstance>  fire;
	std::vector<SmokeInstance> smoke;

	// --- Custom actor spawning ---
	/// Paths loaded async; onSpawnActors fires once all are in cache.
	std::vector<std::string>             spawnMeshPaths;
	std::function<void(Hominem::Scene&)> onSpawnActors;
	/// If set, F3 shows this panel (character tweaker).
	std::function<void()>                onImGuiCharacters;

	// --- Lifecycle ---
	std::function<void()>                             onComplete;
	std::function<std::unique_ptr<Hominem::Layer>()>  nextLayer;
};
