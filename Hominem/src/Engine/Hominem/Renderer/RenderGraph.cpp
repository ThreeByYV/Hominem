#include "hmnpch.h"
#include "RenderGraph.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Hominem/Renderer/RenderCommand.h"
#include <algorithm>

namespace Hominem {

void RenderGraph::AddPass(std::string name, PipelineState state, PassBuilder io, PassFn fn)
{
	m_Passes.push_back({ std::move(name), state, std::move(io), std::move(fn) });
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
	ResetBlackboard();

	std::vector<CommandList> passCmds;
	passCmds.reserve(m_Passes.size());

	for (auto& pass : m_Passes)
	{
		auto cmd = RenderCommand::GetCommandList();
		cmd.SetPipelineState(pass.state);
		cmd.SetDeclaredState(pass.state);

		// bind declared write target — FBO + matching viewport
		if (pass.io.writeFBO)
		{
			auto fbo = GetFBO(*pass.io.writeFBO);
			if (fbo)
			{
				const auto& spec = fbo->GetSpecification();
				cmd.BindFramebuffer(fbo->GetRendererID());
				cmd.SetViewport(0, 0, spec.Width, spec.Height);
			}
		}

		// bind declared texture reads
		for (const auto& b : pass.io.reads)
			cmd.BindTexture(b.slot, ResolveResource(b.name));

		pass.fn(*this, frame, cmd);

		// unbind declared reads, then write target
		for (const auto& b : pass.io.reads)
			cmd.BindTexture(b.slot, 0);
		if (pass.io.writeFBO)
			cmd.BindFramebuffer(0);

		passCmds.push_back(std::move(cmd));
	}

	return passCmds;
}

uint32_t RenderGraph::ResolveResource(const std::string& name)
{
	auto dot = name.find('.');
	HMN_CORE_ASSERT(dot != std::string::npos,
		"RenderGraph: resource '{}' missing attachment suffix (.color / .color1)", name);
	auto fbo = GetFBO(name.substr(0, dot));
	if (!fbo) return 0;
	auto suffix = name.substr(dot + 1);
	if (suffix == "color")  return fbo->GetColorAttachmentRendererID(0);
	if (suffix == "color1") return fbo->GetColorAttachmentRendererID(1);
	HMN_CORE_ASSERT(false, "RenderGraph: unknown attachment suffix '{}' in '{}'", suffix, name);
	return 0;
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
