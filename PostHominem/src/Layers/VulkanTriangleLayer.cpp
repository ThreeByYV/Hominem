#include "hmnpch.h"
#include "VulkanTriangleLayer.h"
#include "LoadingLayer.h"
#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Renderer/RenderFrame.h"

using namespace Hominem;

VulkanTriangleLayer::VulkanTriangleLayer()
    : Layer("VulkanTriangle")
{
}

void VulkanTriangleLayer::OnAttach()
{
    if (const auto result =
        AssetManager::Load<ShaderSource>("game://Shaders/gradient.comp"))
        m_GradientShader = *result;
}

void VulkanTriangleLayer::OnEvent(Event& e)
{
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& ev)
    {
        if (ev.GetKeyCode() == HMN_KEY_1)
            TransitionTo<LoadingLayer>();
        return false;
    });
}

void VulkanTriangleLayer::OnDetach()
{
    m_GradientShader = {};
}

void VulkanTriangleLayer::OnBuildRenderFrame(RenderFrame& frame)
{
    SetFullscreenClear(frame, { 0.f, 0.f, 0.f, 1.f });

    if (!m_GradientShader.IsLoaded()) return;

    frame.vulkanPasses.push_back({
        .shaderSource = m_GradientShader.Get()->source,
        .debugName    = "VulkanGradient",
    });
}
