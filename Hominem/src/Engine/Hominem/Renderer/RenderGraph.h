#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include "Hominem/Renderer/Framebuffer.h"
#include "Hominem/Renderer/CommandList.h"

namespace Hominem {

struct RenderFrame;

class RenderGraph
{
public:
	using PassFn = std::function<void(RenderGraph&, const RenderFrame&, CommandList&)>;

	/// Appends a named pass. The graph calls SetPipelineState(state) before invoking fn each frame.
	void AddPass(std::string name, PipelineState state, PassFn fn);

	/// Registers a named render target. scale is relative to the viewport (1.0 = full res, 0.25 = quarter res).
	/// The FBO is created on the first Execute() with a valid viewport.
	void AddFBO(std::string name, FramebufferFormat format, float scale = 1.0f, uint32_t numColorAttachments = 1);

	/// Returns a previously declared render target. Asserts if the name was never declared.
	Ref<Framebuffer> GetFBO(const std::string& name);

	/// Records every pass in order into its own CommandList. Pure CPU work — safe to
	/// call from the main thread. FBOs are not resized here; call Resize()/SetRenderScale()
	/// from the render thread (after submitting this frame's CommandLists) instead.
	std::vector<CommandList> Record(const RenderFrame& frame);

	/// Explicitly resize all FBOs to the given dimensions (no-op if unchanged).
	/// Makes GL calls — render thread only.
	void Resize(uint32_t w, uint32_t h);

	/// Sets a global render scale (0.25–1.0) applied to all FBO dimensions.
	/// Rebuilds FBOs only when scale changes. Makes GL calls — render thread only.
	void  SetRenderScale(float scale);
	float GetRenderScale() const { return m_RenderScale; }

private:
	void OnResize(uint32_t w, uint32_t h);

	struct Pass     { std::string name; PipelineState state; PassFn fn; };
	struct FBOEntry { Ref<Framebuffer> fbo; FramebufferFormat format; float scale = 1.0f; uint32_t numColorAttachments = 1; };

	std::vector<Pass>                         m_Passes;
	std::unordered_map<std::string, FBOEntry> m_FBOs;
	uint32_t m_Width       = 0;
	uint32_t m_Height      = 0;
	float    m_RenderScale = 1.0f;
};

}
