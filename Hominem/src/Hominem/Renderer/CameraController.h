#pragma once

#include <glm/glm.hpp>

#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Events/MouseEvent.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Camera.h"

namespace Hominem {

    template<typename CameraType>
    class CameraController
    {
    public:
        CameraController() = default;
        virtual ~CameraController() = default;

        virtual void OnUpdate(Timestep ts) = 0;
        virtual void OnEvent(Event& e) = 0;

        const CameraType& GetCamera() const { return m_Camera; }
        CameraType& GetCamera() { return m_Camera; }

        float GetSpeed() const { return m_Speed; }
        void SetSpeed(float speed) { m_Speed = speed; }

    protected:
        CameraType m_Camera;
        float m_Speed = 100.0f;
    };

    class OrthographicCameraController : public CameraController<OrthographicCamera> 
    {
    public:
        OrthographicCameraController(float aspectRatio, bool hasRotation = false);

        void OnUpdate(Timestep ts) override;
        void OnEvent(Event& e) override;

        float GetZoomLevel() const { return m_ZoomLevel; }
        void SetZoomLevel(float level) { m_ZoomLevel = level; }

        const glm::vec3& GetCameraPosition() const { return m_CameraPosition; }
        void SetCameraPosition(const glm::vec3& position) { m_CameraPosition = position; }

        float GetCameraRotation() const { return m_CameraRotation; }
        void SetCameraRotation(float rotation) { m_CameraRotation = rotation; }

    private:
        bool OnMouseScrolled(MouseScrolledEvent& e);
        bool OnWindowResized(WindowResizeEvent& e);

        float m_AspectRatio;
        float m_ZoomLevel = 1.0f;
        bool m_Rotation;

        glm::vec3 m_CameraPosition = { 0.0f, 0.0f, 0.0f };
        float m_CameraRotation = 0.0f;
        float m_CameraTranslationSpeed = 5.0f;
        float m_CameraRotationSpeed = 180.0f;
    };

    class PerspectiveCameraController : public CameraController<PerspectiveCamera>
    {
    public:
        PerspectiveCameraController(float aspectRatio, bool enableMouseControl = true);

        void OnUpdate(Timestep ts) override;
        void OnEvent(Event& e) override;

        const glm::vec3& GetPosition() const { return m_Position; }
        void SetPosition(const glm::vec3& position) { m_Position = position; }
        void SetPosition(float x, float y, float z);

        const glm::vec3& GetTarget() const { return m_Target; }
        const glm::vec3& GetUp() const { return m_Up; }

        float GetMouseSensitivity() const { return m_MouseSensitivity; }
        void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }

    private:
        void Init();
        void Update();
        bool OnMouseMoved(MouseMovedEvent& e);
        bool OnMouseScrolled(MouseScrolledEvent& e);
        bool OnWindowResized(WindowResizeEvent& e);

        uint32_t m_WindowWidth = 800;
        uint32_t m_WindowHeight = 600;
        float m_AspectRatio;
        bool m_EnableMouseControl;

        glm::vec3 m_Position = { 0.0f, 0.0f, -3.0f };
        glm::vec3 m_Target = { 0.0f, 0.0f, 1.0f };
        glm::vec3 m_Up = { 0.0f, 1.0f, 0.0f };

        glm::vec2 m_MousePosition = { 0.0f, 0.0f };
        float m_MouseSensitivity = 0.1f;

        float m_AngleH = 0.0f;
        float m_AngleV = 0.0f;

        float m_FOV = 45.0f;
        float m_NearPlane = 0.1f;
        float m_FarPlane = 1000.0f;
    };
}