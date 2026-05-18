#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include "Hominem/Renderer/Framebuffer.h"

namespace Hominem {

struct RenderFrame;

class RenderGraph
{
public:
	using PassFn = std::function<void(RenderGraph&, const RenderFrame&)>;

	/// Appends a named pass that executes in insertion order each frame.
	void AddPass(std::string name, PassFn fn);

	/// Registers a named render target. scale is relative to the viewport (1.0 = full res, 0.25 = quarter res).
	/// The FBO is created on the first Execute() with a valid viewport.
	void CreateRenderTarget(std::string name, FramebufferFormat format, float scale = 1.0f);

	/// Returns a previously declared render target. Asserts if the name was never declared.
	Ref<Framebuffer> GetFBO(const std::string& name);

	/// Resizes all FBOs if the viewport changed, then runs every pass in order.
	void Execute(const RenderFrame& frame);

private:
	/// Resizes all managed FBOs to the new dimensions.
	void OnResize(uint32_t w, uint32_t h);

	struct Pass     { std::string name; PassFn fn; };
	struct FBOEntry { Ref<Framebuffer> fbo; FramebufferFormat format; float scale = 1.0f; };

	std::vector<Pass>                         m_Passes;
	std::unordered_map<std::string, FBOEntry> m_FBOs;
	uint32_t m_Width  = 0;
	uint32_t m_Height = 0;
};

}
