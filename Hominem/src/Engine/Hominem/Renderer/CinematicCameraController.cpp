#include "hmnpch.h"
#include "CinematicCameraController.h"
#include "Hominem/Events/MouseEvent.h"
#include "Hominem/Events/ApplicationEvent.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

namespace Hominem {

    CinematicCameraController::CinematicCameraController(float aspectRatio)
        : m_AspectRatio(aspectRatio)
    {
        RecalculateProjection();
    }

    void CinematicCameraController::Init(Camera& camera, glm::vec3& cameraPos, glm::vec3& cameraFront,
                                          uint32_t viewportW, uint32_t viewportH, const glm::vec3& initialPos,
                                          float visibleHeight, float leadStrength, float yDeadZone, float smoothing,
                                          float yBias)
    {
        SetCameraTarget(&camera, &cameraPos);
        SetDefaultFraming({ 0.f, yBias, initialPos.z }, visibleHeight);
        LeadStrength = leadStrength;
        YDeadZone    = yDeadZone;
        SetSmoothingFactor(smoothing);

        // Wide near/far range: near=0 would clip anything level with the camera, and the
        // player/level can sit tens of units in front of camera's Z placement (k_CameraZOffset).
        camera.SetOrthographic(visibleHeight, 0.1f, 1000.f);
        camera.SetViewportSize(viewportW, viewportH);
        cameraFront = { 0.f, 0.f, -1.f };
        cameraPos   = initialPos;
    }

    void CinematicCameraController::OnUpdate(Timestep ts)
    {
        if (!m_InCinematic)
            CheckCinematicTriggers();

        if (m_InCinematic && m_ActiveCinematic)
        {
            m_CinematicProgress += ts / m_ActiveCinematic->Duration;

            if (m_ActiveCinematic->AutoMovePlayer)
            {
                float range = m_ActiveCinematic->EndX - m_ActiveCinematic->StartX;
                m_CurrentPlayerPos.x = m_ActiveCinematic->StartX + (range * m_CinematicProgress);
            }

            CameraPoint point = InterpolateCameraPoint(m_CurrentPlayerPos.x);
            m_TargetPosition  = glm::vec3(m_CurrentPlayerPos.x, m_CurrentPlayerPos.y, 0.f) + point.CameraOffset;

            if (std::abs(m_ZoomLevel - point.CameraZoom) > 0.01f)
            {
                m_ZoomLevel = point.CameraZoom;
                RecalculateProjection();
            }

            if (m_CinematicProgress >= 1.0f)
                EndCinematic();
            else
                m_SmoothingFactor = 0.05f;
        }

        if (!m_CameraPosition) return;

        if (m_InCinematic)
        {
            // Cinematic paths already computed a specific target above — no dead zone.
            *m_CameraPosition = glm::mix(*m_CameraPosition, m_TargetPosition, m_SmoothingFactor);
            return;
        }

        m_CameraPosition->x = glm::mix(m_CameraPosition->x, m_TargetPosition.x, m_SmoothingFactor);

        const float deltaY    = m_TargetPosition.y - m_CameraPosition->y;
        const float absDeltaY = glm::abs(deltaY);
        if (absDeltaY > YDeadZone)
            m_CameraPosition->y = glm::mix(m_CameraPosition->y,
                m_TargetPosition.y - glm::sign(deltaY) * YDeadZone, m_SmoothingFactor);

        m_CameraPosition->z = m_TargetPosition.z;
    }

    void CinematicCameraController::Snap()
    {
        if (m_CameraPosition)
            *m_CameraPosition = m_TargetPosition;
    }

    void CinematicCameraController::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseScrolledEvent>(HMN_BIND_EVENT_FN(CinematicCameraController::OnMouseScrolled));
        dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(CinematicCameraController::OnWindowResized));
    }

    void CinematicCameraController::AddCameraPoint(float playerX, glm::vec3 offset, float zoom)
    {
        CameraPoint point;
        point.PlayerX     = playerX;
        point.CameraOffset = offset;
        point.CameraZoom  = zoom;
        m_CameraPoints.push_back(point);

        std::sort(m_CameraPoints.begin(), m_CameraPoints.end(),
            [](const CameraPoint& a, const CameraPoint& b) { return a.PlayerX < b.PlayerX; });
    }

    void CinematicCameraController::AddCinematicSequence(const std::string& name, float startX, float endX,
                                                          float duration, bool autoMovePlayer, float playerSpeed)
    {
        CinematicSequence seq;
        seq.Name          = name;
        seq.StartX        = startX;
        seq.EndX          = endX;
        seq.Duration      = duration;
        seq.AutoMovePlayer = autoMovePlayer;
        seq.PlayerSpeed   = playerSpeed;
        m_Cinematics.push_back(seq);
    }

    bool CinematicCameraController::LoadFromFile(const std::string& path, const std::string& key)
    {
        try
        {
            std::ifstream file(path);
            if (!file.is_open())
            {
                HMN_CORE_ERROR("CinematicCameraController: cannot open '{}'", path);
                return false;
            }

            auto j = nlohmann::json::parse(file);

            std::stringstream ss(key);
            std::string token;
            while (std::getline(ss, token, '.'))
            {
                if (!j.contains(token))
                {
                    HMN_CORE_ERROR("CinematicCameraController: key '{}' not found in '{}'", key, path);
                    return false;
                }
                j = j[token];
            }

            CameraSequenceData data = j.get<CameraSequenceData>();

            for (const auto& p : data.Points)
                AddCameraPoint(p.PlayerX, p.CameraOffset, p.CameraZoom);

            for (const auto& c : data.Cinematics)
            {
                HMN_CORE_INFO("Loading cinematic: '{}' ({:.1f}s)", c.Name, c.Duration);
                AddCinematicSequence(c.Name, c.StartX, c.EndX, c.Duration, c.AutoMovePlayer, c.PlayerSpeed);
            }

            SetSmoothingFactor(data.Smoothing);
            HMN_CORE_INFO("Camera config loaded: {} points, {} cinematics",
                data.Points.size(), data.Cinematics.size());
            return true;
        }
        catch (const nlohmann::json::exception& e)
        {
            HMN_CORE_ERROR("CinematicCameraController: JSON error '{}': {}", path, e.what());
            return false;
        }
    }

    void CinematicCameraController::TriggerCinematic(const std::string& name, std::function<void()> onEnd)
    {
        for (auto& cinematic : m_Cinematics)
        {
            if (cinematic.Name == name)
            {
                HMN_CORE_INFO("Cinematic started: '{}'", name);
                m_InCinematic      = true;
                m_ActiveCinematic  = &cinematic;
                m_CinematicProgress = 0.0f;
                m_OnCinematicEnd   = std::move(onEnd);
                return;
            }
        }
        HMN_CORE_WARN("CinematicCameraController: cinematic '{}' not found", name);
    }

    void CinematicCameraController::EndCinematic()
    {
        if (m_ActiveCinematic)
            HMN_CORE_INFO("Cinematic ended: '{}'", m_ActiveCinematic->Name);

        m_InCinematic      = false;
        m_ActiveCinematic  = nullptr;
        m_CinematicProgress = 0.0f;
        m_SmoothingFactor  = 0.15f;

        if (m_OnCinematicEnd)
        {
            auto cb = std::move(m_OnCinematicEnd);
            m_OnCinematicEnd = nullptr;
            cb();
        }
    }

    void CinematicCameraController::CheckCinematicTriggers()
    {
        for (auto& cinematic : m_Cinematics)
        {
            if (cinematic.HasTriggered) continue;

            if (m_CurrentPlayerPos.x >= cinematic.StartX &&
                m_CurrentPlayerPos.x <= cinematic.StartX + 1.0f)
            {
                cinematic.HasTriggered = true;

                std::function<void()> cb = nullptr;
                auto it = m_CinematicCallbacks.find(cinematic.Name);
                if (it != m_CinematicCallbacks.end())
                    cb = it->second;

                TriggerCinematic(cinematic.Name, cb);
                return;
            }
        }
    }

    void CinematicCameraController::RegisterOnComplete(const std::string& name, std::function<void()> cb)
    {
        m_CinematicCallbacks[name] = std::move(cb);

        for (auto& cinematic : m_Cinematics)
        {
            if (cinematic.Name == name)
            {
                cinematic.HasTriggered = false;
                break;
            }
        }
    }

    void CinematicCameraController::UpdateCameraForPlayer(glm::vec2 playerPos, glm::vec2 velocity)
    {
        m_CurrentPlayerPos = playerPos;

        if (m_InCinematic)
            return;

        CameraPoint point = (m_CameraPoints.size() >= 2)
            ? InterpolateCameraPoint(playerPos.x)
            : CameraPoint{ playerPos.x, m_DefaultOffset, m_DefaultZoom };

        // Projected-focus: lead the camera in the direction of travel.
        const float leadX = playerPos.x + velocity.x * LeadStrength;
        m_TargetPosition  = glm::vec3(leadX, playerPos.y, 0.f) + point.CameraOffset;

        if (std::abs(m_ZoomLevel - point.CameraZoom) > 0.01f)
        {
            m_ZoomLevel = point.CameraZoom;
            RecalculateProjection();
        }
    }

    CameraPoint CinematicCameraController::InterpolateCameraPoint(float playerX) const
    {
        if (playerX <= m_CameraPoints.front().PlayerX) return m_CameraPoints.front();
        if (playerX >= m_CameraPoints.back().PlayerX)  return m_CameraPoints.back();

        for (size_t i = 0; i < m_CameraPoints.size() - 1; i++)
        {
            if (playerX >= m_CameraPoints[i].PlayerX && playerX < m_CameraPoints[i + 1].PlayerX)
            {
                const CameraPoint& p1 = m_CameraPoints[i];
                const CameraPoint& p2 = m_CameraPoints[i + 1];
                float t = (playerX - p1.PlayerX) / (p2.PlayerX - p1.PlayerX);

                CameraPoint result;
                result.PlayerX     = playerX;
                result.CameraOffset = glm::mix(p1.CameraOffset, p2.CameraOffset, t);
                result.CameraZoom  = glm::mix(p1.CameraZoom,  p2.CameraZoom,  t);
                return result;
            }
        }
        return m_CameraPoints.back();
    }

    void CinematicCameraController::SetZoomLevel(float level)
    {
        m_ZoomLevel = level;
        RecalculateProjection();
    }

    void CinematicCameraController::RecalculateProjection()
    {
        if (m_SceneCamera)
            m_SceneCamera->SetOrthographicSize(m_ZoomLevel);
    }

    bool CinematicCameraController::OnMouseScrolled(MouseScrolledEvent& e)
    {
        m_ZoomLevel -= e.GetYOffset() * 0.25f;
        m_ZoomLevel  = glm::clamp(m_ZoomLevel, 0.25f, 20.0f);
        RecalculateProjection();
        return false;
    }

    bool CinematicCameraController::OnWindowResized(WindowResizeEvent& e)
    {
        m_AspectRatio = (float)e.GetWidth() / (float)e.GetHeight();
        RecalculateProjection();
        return false;
    }

}
