#pragma once
#include "CutsceneDesc.h"

namespace IntroCutscene {
	/// Virtual asset path for the 3D set — published so LoadingLayer can preload it.
	inline constexpr const char* k_SetMesh = "game://Textures/facade_san_nicolas.glb";
	/// Virtual asset paths for the silhouette runner/idle meshes — shared between
	/// CutsceneDesc::spawnMeshPaths (preload) and SpawnIntroCharacters (actual spawn).
	inline constexpr const char* k_RunMesh  = "game://Textures/Running.fbx";
	inline constexpr const char* k_IdleMesh = "game://Textures/Idle.fbx";
	CutsceneDesc Make();
}
