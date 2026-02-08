#include "hmnpch.h"
#include "CinematicCameraController.h"
#include "Hominem/Events/MouseEvent.h"
#include "Hominem/Events/ApplicationEvent.h"

namespace Hominem {

    CinematicCameraController::CinematicCameraController(float aspectRatio)
        : m_AspectRatio(aspectRatio)
    {
        RecalculateProjection();
    }

    void CinematicCameraController::OnUpdate(Timestep ts)
    {
        // Check if we should trigger a cinematic
        if (!m_InCinematic)
        {
            CheckCinematicTriggers();
        }

        // Update cinematic sequence
        if (m_InCinematic && m_ActiveCinematic)
        {
            m_CinematicProgress += ts / m_ActiveCinematic->Duration;

            // Auto-move player during cinematic if enabled
            if (m_ActiveCinematic->AutoMovePlayer)
            {
                float range = m_ActiveCinematic->EndX - m_ActiveCinematic->StartX;
                m_CurrentPlayerPos.x = m_ActiveCinematic->StartX + (range * m_CinematicProgress);
            }

            // Get camera position for current player X
            CameraPoint point = InterpolateCameraPoint(m_CurrentPlayerPos.x);
            m_TargetPosition = glm::vec3(m_CurrentPlayerPos.x, m_CurrentPlayerPos.y, 0.0f) + point.CameraOffset;

            // Update zoom
            if (std::abs(m_ZoomLevel - point.CameraZoom) > 0.01f)
            {
                m_ZoomLevel = point.CameraZoom;
                RecalculateProjection();
            }

            // End cinematic when duration complete
            if (m_CinematicProgress >= 1.0f)
            {
                EndCinematic();
            }
            else
            {
                // Slower smoothing during cutscene for cinematic feel
                m_SmoothingFactor = 0.05f;
            }
        }

        // Smooth camera movement towards target (lerp)
        glm::vec3 currentPos = m_Camera.GetPosition();
        glm::vec3 smoothPos = glm::mix(currentPos, m_TargetPosition, m_SmoothingFactor);
        m_Camera.SetPosition(smoothPos);
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
        point.PlayerX = playerX;
        point.CameraOffset = offset;
        point.CameraZoom = zoom;

        m_CameraPoints.push_back(point);

        // Sort by PlayerX for proper interpolation
        std::sort(m_CameraPoints.begin(), m_CameraPoints.end(),
            [](const CameraPoint& a, const CameraPoint& b) {
                return a.PlayerX < b.PlayerX;
            });
    }

    void CinematicCameraController::AddCinematicSequence(const std::string& name, float startX, float endX,
                                                         float duration, bool autoMovePlayer, float playerSpeed)
    {
        CinematicSequence sequence;
        sequence.Name = name;
        sequence.StartX = startX;
        sequence.EndX = endX;
        sequence.Duration = duration;
        sequence.AutoMovePlayer = autoMovePlayer;
        sequence.PlayerSpeed = playerSpeed;

        m_Cinematics.push_back(sequence);
    }

    void CinematicCameraController::TriggerCinematic(const std::string& name)
    {
        for (auto& cinematic : m_Cinematics)
        {
            if (cinematic.Name == name)
            {
                HMN_CORE_WARN("🎬 CINEMATIC STARTED: '{}' at x={:.2f} (duration={:.1f}s, autoMove={})",
                    name, m_CurrentPlayerPos.x, cinematic.Duration, cinematic.AutoMovePlayer);

                m_InCinematic = true;
                m_ActiveCinematic = &cinematic;
                m_CinematicProgress = 0.0f;
                return;
            }
        }
    }

    void CinematicCameraController::EndCinematic()
    {
        if (m_ActiveCinematic)
        {
            HMN_CORE_INFO("🎬 CINEMATIC ENDED: '{}' at x={:.2f}", m_ActiveCinematic->Name, m_CurrentPlayerPos.x);
        }

        m_InCinematic = false;
        m_ActiveCinematic = nullptr;
        m_CinematicProgress = 0.0f;
        m_SmoothingFactor = 0.15f;  // Return to normal smoothing
    }

    void CinematicCameraController::CheckCinematicTriggers()
    {
        for (auto& cinematic : m_Cinematics)
        {
            // Log when approaching trigger (1 unit before)
            static std::unordered_map<std::string, bool> s_ApproachLogged;
            if (m_CurrentPlayerPos.x >= cinematic.StartX - 1.0f &&
                m_CurrentPlayerPos.x < cinematic.StartX &&
                !s_ApproachLogged[cinematic.Name])
            {
                HMN_CORE_WARN("Approaching cinematic '{}' trigger at x={:.1f}! Current x={:.2f}",
                    cinematic.Name, cinematic.StartX, m_CurrentPlayerPos.x);
                s_ApproachLogged[cinematic.Name] = true;
            }

            // Trigger when player enters the zone (1 unit tolerance)
            if (m_CurrentPlayerPos.x >= cinematic.StartX &&
                m_CurrentPlayerPos.x <= cinematic.StartX + 1.0f)
            {
                TriggerCinematic(cinematic.Name);
                return;
            }
        }
    }

    void CinematicCameraController::UpdateCameraForPlayer(glm::vec2 playerPos)
    {
        if (m_CameraPoints.size() < 2)
            return;

        m_CurrentPlayerPos = playerPos;

        // Only update target if not in cinematic
        if (!m_InCinematic)
        {
            CameraPoint point = InterpolateCameraPoint(playerPos.x);
            m_TargetPosition = glm::vec3(playerPos.x, playerPos.y, 0.0f) + point.CameraOffset;

            if (std::abs(m_ZoomLevel - point.CameraZoom) > 0.01f)
            {
                m_ZoomLevel = point.CameraZoom;
                RecalculateProjection();
            }
        }
    }

    CameraPoint CinematicCameraController::InterpolateCameraPoint(float playerX) const
    {
        // Before first point
        if (playerX <= m_CameraPoints.front().PlayerX)
            return m_CameraPoints.front();

        // After last point
        if (playerX >= m_CameraPoints.back().PlayerX)
            return m_CameraPoints.back();

        // Find surrounding points for linear interpolation
        for (size_t i = 0; i < m_CameraPoints.size() - 1; i++)
        {
            if (playerX >= m_CameraPoints[i].PlayerX && playerX < m_CameraPoints[i + 1].PlayerX)
            {
                const CameraPoint& p1 = m_CameraPoints[i];
                const CameraPoint& p2 = m_CameraPoints[i + 1];

                // Calculate interpolation factor (0 to 1)
                float t = (playerX - p1.PlayerX) / (p2.PlayerX - p1.PlayerX);

                // Linear interpolation
                CameraPoint result;
                result.PlayerX = playerX;
                result.CameraOffset = glm::mix(p1.CameraOffset, p2.CameraOffset, t);
                result.CameraZoom = glm::mix(p1.CameraZoom, p2.CameraZoom, t);

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
        float orthoLeft = -m_AspectRatio * m_ZoomLevel;
        float orthoRight = m_AspectRatio * m_ZoomLevel;
        float orthoBottom = -m_ZoomLevel;
        float orthoTop = m_ZoomLevel;

        m_Camera.SetProjection(orthoLeft, orthoRight, orthoBottom, orthoTop);
    }

    bool CinematicCameraController::OnMouseScrolled(MouseScrolledEvent& e)
    {
        m_ZoomLevel -= e.GetYOffset() * 0.25f;
        m_ZoomLevel = glm::clamp(m_ZoomLevel, 0.25f, 20.0f);
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