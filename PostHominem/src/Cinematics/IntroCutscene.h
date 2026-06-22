#pragma once
#include "CutsceneDesc.h"

namespace IntroCutscene {
	/// Virtual asset path for the 3D set — published so LoadingLayer can preload it.
	inline constexpr const char* k_SetMesh = "game://Textures/facade_san_nicolas.glb";
	CutsceneDesc Make();
}
