#pragma once

#include <cstdint>

namespace Hominem {

	/// Renderer settings that game code can read and write without depending on Renderer3D.
	/// The renderer reads these each frame; layers write them directly.
	struct RenderSettings
	{
		// Debug geometry
		static inline bool  DrawNormals      = false;
		static inline float NormalLength     = 0.1f;
		static inline bool  DrawAABB         = false;
		static inline bool  DrawBoneWeights  = false;
		static inline int   DisplayBoneIndex = 0;

		// Shading modes
		static inline bool ToonShading  = false;
		static inline bool DebugHeatmap = false;

		// Features
		static inline bool AreaLights = true;

		// Set by the renderer during Init based on GPU detection; read by game code.
		static inline float RecommendedRenderScale = 1.0f;

		/// Routes a full shader reload through the render thread. Safe to call from any thread.
		static void RequestShaderReload();
	};

}
