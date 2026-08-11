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
		static inline bool DDGIDebug    = false;  // shade with the raw DDGI irradiance only

		// Features
		static inline bool AreaLights = true;

		// Temporal AA
		static inline bool  TAA            = true;
		static inline bool  TAAVelocity    = true;  // off = camera-only reprojection from depth
		static inline float TAAFeedback    = 0.9f;  // history weight; higher = more samples, more ghosting
		static inline float TAAJitterScale = 1.0f;  // 0 keeps the sample at the pixel centre (no AA)
		static inline int   TAADebugView   = 0;     // 0 off, 1 reprojection offset, 2 clamped history

		// Set by the renderer during Init based on GPU detection; read by game code.
		static inline float RecommendedRenderScale = 1.0f;

		/// Routes a full shader reload through the render thread. Safe to call from any thread.
		static void RequestShaderReload();
	};

}
