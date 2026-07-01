#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Assets/AssetHandle.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Renderer/ShaderSource.h"

class VulkanTriangleLayer : public Hominem::Layer
{
public:
    VulkanTriangleLayer();

protected:
    void OnAttach() override;
    void OnDetach() override;
    void OnBuildRenderFrame(Hominem::RenderFrame& frame) override;
    void OnEvent(Hominem::Event& e) override;

private:
    Hominem::AssetHandle<Hominem::ShaderSource> m_GradientShader;
};
