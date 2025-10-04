#include "hmnpch.h"
#include "CameraController.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"

namespace Hominem {

    OrthographicCameraController::OrthographicCameraController(float aspectRatio, bool hasRotation)
        : m_AspectRatio(aspectRatio), m_Rotation(hasRotation)
    {
        this->m_Camera = OrthographicCamera(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
    }

    void OrthographicCameraController::OnUpdate(Timestep ts)
    {
        if (Input::IsKeyPressed(HMN_KEY_A))
            m_CameraPosition.x -= m_CameraTranslationSpeed * ts;

        else if (Input::IsKeyPressed(HMN_KEY_D))
            m_CameraPosition.x += m_CameraTranslationSpeed * ts;

        if (Input::IsKeyPressed(HMN_KEY_W))
            m_CameraPosition.y += m_CameraTranslationSpeed * ts;

        else if (Input::IsKeyPressed(HMN_KEY_S))
            m_CameraPosition.y -= m_CameraTranslationSpeed * ts;

        if (m_Rotation)
        {
            if (Input::IsKeyPressed(HMN_KEY_Q))
                m_CameraRotation += m_CameraRotationSpeed * ts;
            if (Input::IsKeyPressed(HMN_KEY_E))
                m_CameraRotation -= m_CameraRotationSpeed * ts;
            this->m_Camera.SetRotation(m_CameraRotation);
        }
        this->m_Camera.SetPosition(m_CameraPosition);
        m_CameraTranslationSpeed = m_ZoomLevel;
    }

    void OrthographicCameraController::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseScrolledEvent>(HMN_BIND_EVENT_FN(OrthographicCameraController::OnMouseScrolled));
        dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(OrthographicCameraController::OnWindowResized));
    }

    bool OrthographicCameraController::OnMouseScrolled(MouseScrolledEvent& e)
    {
        m_ZoomLevel -= e.GetYOffset() * 0.25f;
        m_ZoomLevel = std::max(m_ZoomLevel, 0.25f);
        this->m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
        return false;
    }

    bool OrthographicCameraController::OnWindowResized(WindowResizeEvent& e)
    {
        m_AspectRatio = (float)e.GetWidth() / (float)e.GetHeight();
        this->m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
        return false;
    }


    // PERSPECTIVE CAMERA BELOW
    PerspectiveCameraController::PerspectiveCameraController(float aspectRatio, bool enableMouseControl)
        : m_AspectRatio(aspectRatio), m_EnableMouseControl(enableMouseControl)
    {
        m_WindowWidth = 800;
        m_WindowHeight = 600;
        this->m_Camera = PerspectiveCamera(m_FOV, m_AspectRatio, m_NearPlane, m_FarPlane);
        Init();
    }

    void PerspectiveCameraController::SetPosition(float x, float y, float z)
    {
        m_Position.x = x;
        m_Position.y = y;
        m_Position.z = z;
    }

    bool PerspectiveCameraController::OnMouseMoved(MouseMovedEvent& e)
    {
        if (!m_EnableMouseControl) return false;

        float deltaX = e.GetX() - m_MousePosition.x;
        float deltaY = e.GetY() - m_MousePosition.y;

        m_MousePosition.x = e.GetX();
        m_MousePosition.y = e.GetY();

        m_AngleH -= deltaX * m_MouseSensitivity;
        m_AngleV += deltaY * m_MouseSensitivity;

        if (m_AngleV > 89.0f) m_AngleV = 89.0f;
        if (m_AngleV < -89.0f) m_AngleV = -89.0f;

        Update();
        return false;
    }

    bool PerspectiveCameraController::OnMouseScrolled(MouseScrolledEvent& e)
    {
        m_FOV -= e.GetYOffset() * 2.0f;
        m_FOV = glm::clamp(m_FOV, 1.0f, 120.0f);
        this->m_Camera.SetProjection(m_FOV, m_AspectRatio, m_NearPlane, m_FarPlane);
        return false;
    }

    bool PerspectiveCameraController::OnWindowResized(WindowResizeEvent& e)
    {
        m_AspectRatio = (float)e.GetWidth() / (float)e.GetHeight();
        m_WindowWidth = e.GetWidth();
        m_WindowHeight = e.GetHeight();
        this->m_Camera.SetProjection(m_FOV, m_AspectRatio, m_NearPlane, m_FarPlane);
        return false;
    }

    void PerspectiveCameraController::Init()
    {
        glm::vec3 hTarget{ m_Target.x, 0.0f, m_Target.z };
        hTarget = glm::normalize(hTarget);
        float angle = glm::degrees(asin(abs(hTarget.z)));

        if (hTarget.z >= 0.0f)
        {
            if (hTarget.x >= 0.0f)
            {
                m_AngleH = 360.0f - angle;
            }
            else
            {
                m_AngleH = 180.0f + angle;
            }
        }
        else
        {
            if (hTarget.x >= 0.0f)
            {
                m_AngleH = angle;
            }
            else
            {
                m_AngleH = 180.0f - angle;
            }
        }

        m_AngleV = -glm::degrees(asin(m_Target.y));

        m_MousePosition.x = m_WindowWidth / 2.0f;
        m_MousePosition.y = m_WindowHeight / 2.0f;

        Update();
    }

    void PerspectiveCameraController::Update()
    {
        glm::vec3 yAxis{ 0.0f, 1.0f, 0.0f };

        glm::vec3 view{ 1.0f, 0.0f, 0.0f };
        glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(m_AngleH), yAxis);
        view = glm::vec3(rotationMatrix * glm::vec4(view, 0.0f));
        view = glm::normalize(view);

        glm::vec3 U = glm::normalize(glm::cross(yAxis, view));
        glm::mat4 vertRotation = glm::rotate(glm::mat4(1.0f), glm::radians(m_AngleV), U);
        view = glm::vec3(vertRotation * glm::vec4(view, 0.0f));

        m_Target = glm::normalize(view);
        m_Up = glm::normalize(glm::cross(m_Target, U));

        this->m_Camera.SetViewMatrix(m_Position, m_Target, m_Up);
    }

    void PerspectiveCameraController::OnUpdate(Timestep ts)
    {
        float velocity = this->m_Speed * ts;

        if (Input::IsKeyPressed(HMN_KEY_A))
        {
            glm::vec3 left = glm::normalize(glm::cross(m_Up, m_Target));
            m_Position += left * velocity;
        }
        else if (Input::IsKeyPressed(HMN_KEY_D))
        {
            glm::vec3 right = glm::normalize(glm::cross(m_Target, m_Up));
            m_Position += right * velocity;
        }
        if (Input::IsKeyPressed(HMN_KEY_W))
        {
            m_Position += m_Target * velocity;
        }
        else if (Input::IsKeyPressed(HMN_KEY_S))
        {
            m_Position -= m_Target * velocity;
        }

        Update();
    }

    void PerspectiveCameraController::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        if (m_EnableMouseControl)
        {
            dispatcher.Dispatch<MouseMovedEvent>(HMN_BIND_EVENT_FN(PerspectiveCameraController::OnMouseMoved));
        }
        dispatcher.Dispatch<MouseScrolledEvent>(HMN_BIND_EVENT_FN(PerspectiveCameraController::OnMouseScrolled));
        dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(PerspectiveCameraController::OnWindowResized));
    }
}