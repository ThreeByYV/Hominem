#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Scene/SceneDesc.h"

namespace Hominem {

/// Base class for layers that own a Scene.
/// Override Describe() to declare static configuration; OnSceneReady() to spawn actors.
/// Boilerplate (scene creation, physics, camera, viewport resize) is handled automatically.
class SceneLayer : public Layer
{
public:
    void OnAttach() final;
    void OnDetach() final;
    void OnEvent(Event& e) override;

protected:
    [[nodiscard]] virtual SceneDesc Describe()                    = 0;
    virtual void OnSceneReady()                                   {}
    virtual void OnSceneDetach()                                  {}
    virtual void OnWindowResized(uint32_t w, uint32_t h)         {}

private:
    bool HandleWindowResize(WindowResizeEvent& e);
};

} // namespace Hominem
