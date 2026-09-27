#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Hominem {

	/// Renderer settings that game code can read and write without depending on ForwardPlusRenderer.
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

		/// Turns high-severity GL errors back into a hard assert. Off by default so a
		/// stricter driver than the one you develop on can't stop the app from starting.
		static inline bool StrictGLErrors = false;

		/// Forces the ray tracing path off even where the device supports it, taking DDGI
		/// with it. Read once during Vulkan device creation, so set it before the
		/// Application is constructed - it does nothing at runtime.
		static inline bool RayTracing = true;

		/// Integrated GPUs are excluded from ray tracing on throughput grounds, not
		/// capability. Set this to test the path on one anyway; expect seconds per frame.
		static inline bool RayTracingOnIntegrated = false;

		// Temporal AA. Feedback min/max are Playdead's shipped values - history weight is
		// interpolated between them by how much the pixel's luminance is changing.
		static inline bool  TAA             = true;
		static inline bool  TAAVelocity     = true;  // off = camera-only reprojection from depth
		static inline bool  TAADilation     = true;  // velocity from the closest fragment of the 3x3
		static inline float TAAFeedbackMin  = 0.88f; // where the pixel is changing
		static inline float TAAFeedbackMax  = 0.97f; // where it is stable
		static inline float TAAJitterScale  = 1.0f;  // 0 keeps the sample at the pixel centre (no AA)
		static inline int   TAADebugView    = 0;     // 0 off, 1 motion, 2 clamped history, 3 rejection

		// Temporal upscaler, by name in render.ini. Read once when the renderer starts.
		// Order matches UpscalerBackend.
		static inline int Upscaler = 0;
		static constexpr const char* UpscalerNames[] = { "taa", "dlss", "fsr", "passthrough", nullptr };

		// Set by the renderer during Init based on GPU detection; read by game code.
		static inline float RecommendedRenderScale = 1.0f;

		/// Routes a full shader reload through the render thread. Safe to call from any thread.
		static void RequestShaderReload();

		/// Lowers RecommendedRenderScale on integrated GPUs. Needs a current GL context.
		static void DetectRecommendedRenderScale();

		/// Drops the accumulated TAA history at the start of the next frame. Call whenever
		/// the image discontinues - a camera cut, a teleport, a scene swap - or the resolve
		/// reprojects across the discontinuity and smears the old shot into the new one.
		/// Consumed by SceneRenderer::PrepareTemporal.
		static void RequestTAAHistoryReset() { s_TAAHistoryResetPending.store(true, std::memory_order_release); }
		static bool ConsumeTAAHistoryReset() { return s_TAAHistoryResetPending.exchange(false, std::memory_order_acq_rel); }

		/// Every setting as "Name=Value", one per line. Embedded in RenderDoc captures so the
		/// state that produced a frame travels with it.
		static std::string Describe();

		/// One line naming only the settings that differ from their compiled-in defaults.
		static void LogAll();

		/// Assigns by name, parsing the value according to the setting's type. Returns false
		/// and leaves the setting alone if the name is unknown or the value doesn't parse.
		static bool Set(std::string_view name, std::string_view value);

		/// Reads -Name=Value and --Name=Value, ignoring everything else on the line.
		static void ApplyArgs(int argc, char** argv);

		/// A missing file is not an error - the defaults stand and SaveTo writes one on exit.
		static void LoadFrom(const std::string& path);
		static void SaveTo(const std::string& path);

		/// One row of the settings table, for UI that walks every setting rather than naming
		/// them one at a time. `value` points straight at the setting, so a widget bound to
		/// it writes through.
		struct Setting
		{
			enum class Type { Bool, Int, Float, Choice };

			const char* name;
			void*       value; // bool*, int* or float*, per type; int* (an index) for Choice
			Type        type;
			bool        isDefault;
			bool        isDerived; // recomputed each launch, so editing it does nothing
			const char* const* choices; // Choice only: null-terminated names
		};

		static std::vector<Setting> Enumerate();
		static void ResetToDefaults();

		/// Takes the current values as the baseline that LogAll reports against and that
		/// ResetToDefaults returns to. Call after the game has stated its own defaults and
		/// before LoadFrom, so the config file still overrides them.
		static void CaptureDefaults();

	private:
		static inline std::atomic<bool> s_TAAHistoryResetPending { false };
	};

}
