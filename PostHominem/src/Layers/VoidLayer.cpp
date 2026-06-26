#include "hmnpch.h"
#include "VoidLayer.h"
#include "SandboxLayer.h"

#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"

using namespace Hominem;

VoidLayer::VoidLayer()
	: Layer("Void")
{
}

void VoidLayer::OnUpdate(Timestep ts)
{
	if (Input::IsKeyPressed(HMN_KEY_3))
	{
		TransitionTo<SandboxLayer>();
		return;
	}
}

void VoidLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	SetFullscreenClear(frame, { 0.6f, 0.1f, 0.2f, 1.0f });
}

void VoidLayer::OnImGuiRender()
{
}
