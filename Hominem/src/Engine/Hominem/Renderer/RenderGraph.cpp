#include "hmnpch.h"
#include "RenderGraph.h"
#include "Hominem/Renderer/RenderFrame.h"

namespace Hominem {

void RenderGraph::AddPass(std::string name, PassFn fn)
{
	m_Passes.push_back({ std::move(name), std::move(fn) });
}

void RenderGraph::CreateRenderTarget(std::string name, FramebufferFormat format)
{
	HMN_CORE_ASSERT(m_FBOs.find(name) == m_FBOs.end(),
		"RenderGraph: target '{}' already declared", name);
	m_FBOs[std::move(name)] = { nullptr, format };
}

Ref<Framebuffer> RenderGraph::GetFBO(const std::string& name)
{
	const auto it = m_FBOs.find(name);
	HMN_CORE_ASSERT(it != m_FBOs.end(),
		"RenderGraph::GetFBO('{}') — target was never declared", name);
	return it->second.fbo;
}

void RenderGraph::Execute(const RenderFrame& frame)
{
	// Only resize FBOs when the viewport is valid — never create a 0-sized FBO.
	// Passes always run so the clear fires every frame and the back buffer is never undefined.
	if (frame.viewportWidth > 0 && frame.viewportHeight > 0)
		if (frame.viewportWidth != m_Width || frame.viewportHeight != m_Height)
			OnResize(frame.viewportWidth, frame.viewportHeight);

	for (auto& pass : m_Passes)
		pass.fn(*this, frame);
}

void RenderGraph::OnResize(uint32_t w, uint32_t h)
{
	m_Width  = w;
	m_Height = h;
	for (auto& [name, entry] : m_FBOs)
	{
		FramebufferSpecification spec;
		spec.Width  = w;
		spec.Height = h;
		spec.Format = entry.format;

		if (entry.fbo)
			entry.fbo->Resize(w, h);
		else
			entry.fbo = Framebuffer::Create(spec);
	}
}

}
