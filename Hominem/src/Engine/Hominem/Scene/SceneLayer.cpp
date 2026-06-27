#include "hmnpch.h"
#include "Hominem/Scene/SceneLayer.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Core/Application.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Audio/AudioSystem.h"

namespace Hominem {

void SceneLayer::OnAttach()
{
    auto desc = Describe();
    m_Scene   = CreateRef<Scene>();

    m_Scene->SetClearColor(desc.ClearColor);
    m_Scene->GetLights() = desc.Lights;

    if (desc.Physics.Enabled)
        m_Scene->SetPhysicsWorld(CreateRef<PhysicsWorld>(desc.Physics.Gravity));

    m_Scene->GetCamera().SetOrthographic(
        desc.Camera.OrthoSize, desc.Camera.Near, desc.Camera.Far);

    m_Scene->SetPostProcess(desc.PostProcess);

    auto& win = Application::Get().GetWindow();
    m_Scene->OnViewportResize(win.GetWidth(), win.GetHeight());

    SceneContext ctx(*m_Scene, AudioSystem::Get(), win.GetWidth(), win.GetHeight());
    OnSceneReady(ctx);
}

void SceneLayer::OnDetach()
{
    OnSceneDetach();
    m_Scene.reset();
}

void SceneLayer::OnEvent(Event& e)
{
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(SceneLayer::HandleWindowResize));
}

bool SceneLayer::HandleWindowResize(WindowResizeEvent& e)
{
    if (m_Scene) m_Scene->OnViewportResize(e.GetWidth(), e.GetHeight());
    OnWindowResized(e.GetWidth(), e.GetHeight());
    return false;
}

} // namespace Hominem
