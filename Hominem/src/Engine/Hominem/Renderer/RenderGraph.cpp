#include "hmnpch.h"
#include "RenderGraph.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Hominem/Renderer/RenderCommand.h"
#include <algorithm>

namespace Hominem {

void RenderGraph::AddPass(std::string name, PipelineState state, PassFn fn)
{
	m_Passes.push_back({ std::move(name), state, std::move(fn) });
}

void RenderGraph::AddFBO(std::string name, FramebufferFormat format, float scale, uint32_t numColorAttachments)
{
	HMN_CORE_ASSERT(m_FBOs.find(name) == m_FBOs.end(),
		"RenderGraph: target '{}' already declared", name);
	m_FBOs[std::move(name)] = { nullptr, format, scale, numColorAttachments };
}

Ref<Framebuffer> RenderGraph::GetFBO(const std::string& name)
{
	const auto it = m_FBOs.find(name);
	HMN_CORE_ASSERT(it != m_FBOs.end(),
		"RenderGraph::GetFBO('{}') — target was never declared", name);
	return it->second.fbo; // may be null if viewport was 0 and OnResize hasn't fired yet
}

void RenderGraph::Resize(uint32_t w, uint32_t h)
{
	if (w > 0 && h > 0 && (w != m_Width || h != m_Height))
		OnResize(w, h);
}

void RenderGraph::SetRenderScale(float scale)
{
	float clamped = std::max(0.25f, std::min(scale, 1.0f));
	if (clamped == m_RenderScale) return;
	m_RenderScale = clamped;
	if (m_Width > 0 && m_Height > 0)
		OnResize(m_Width, m_Height);
}

std::vector<CommandList> RenderGraph::Record(const RenderFrame& frame)
{
	std::vector<CommandList> passCmds;
	passCmds.reserve(m_Passes.size());

	for (auto& pass : m_Passes)
	{
		auto cmd = RenderCommand::GetCommandList();
		cmd.SetPipelineState(pass.state);
		pass.fn(*this, frame, cmd);
		passCmds.push_back(std::move(cmd));
	}

	return passCmds;
}

void RenderGraph::OnResize(uint32_t w, uint32_t h)
{
	m_Width  = w;
	m_Height = h;
	for (auto& [name, entry] : m_FBOs)
	{
		uint32_t fw = std::max(1u, (uint32_t)(w * entry.scale * m_RenderScale));
		uint32_t fh = std::max(1u, (uint32_t)(h * entry.scale * m_RenderScale));

		FramebufferSpecification spec;
		spec.Width               = fw;
		spec.Height              = fh;
		spec.Format              = entry.format;
		spec.NumColorAttachments = entry.numColorAttachments;

		if (entry.fbo)
			entry.fbo->Resize(fw, fh);
		else
			entry.fbo = Framebuffer::Create(spec);
	}
}

}
