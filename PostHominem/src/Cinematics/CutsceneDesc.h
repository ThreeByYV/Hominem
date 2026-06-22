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
	std::string setMeshPath;
	std::string skyboxPath;
	std::string cutsceneJsonPath;

	// --- Scene framing (live-tweakable via the F2 panel) ---
	float     camFOV    = 90.f;
	glm::vec3 camPos    = {};
	glm::vec3 camTarget = { 1.f, 0.f, 0.f };
	glm::vec3 setPos    = {};
	glm::vec3 setScale  = { 1.f, 1.f, 1.f };
	glm::vec3 setRotDeg = {};

	// --- Directional light ---
	glm::vec3 lightDir     = { -0.2f, -1.f, -0.4f };
	glm::vec3 lightColor   = { 1.f, 1.f, 1.f };
	float     lightAmbient = 0.3f;
	float     lightDiffuse = 1.0f;
	float     skyIntensity = 1.0f;

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
	std::function<void()>                             onComplete; // side-effects when cues finish
	std::function<std::unique_ptr<Hominem::Layer>()>  nextLayer;  // factory for the transition target
};
