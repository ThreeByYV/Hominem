#include "hmnpch.h"
#include "CinematicCameraController.h"
#include "Hominem/Events/MouseEvent.h"
#include "Hominem/Events/ApplicationEvent.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include "Hominem/Scene/Components.h"

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

        // Write smoothed position directly into the ECS entity — no external sync needed
        if (m_CameraEntity && m_CameraEntity.HasComponent<TransformComponent>())
        {
            auto& transform = m_CameraEntity.GetComponent<TransformComponent>();
            transform.Translation = glm::mix(transform.Translation, m_TargetPosition, m_SmoothingFactor);
        }
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

            // Navigate dot-separated key path (e.g. "camera_sequences.level_01_vista")
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

            for (const auto& point : data.Points)
                AddCameraPoint(point.PlayerX, point.CameraOffset, point.CameraZoom);

            for (const auto& cinematic : data.Cinematics)
            {
                HMN_CORE_INFO("Loading cinematic: '{}' (StartX={:.1f}, EndX={:.1f}, Duration={:.1f}s)",
                    cinematic.Name, cinematic.StartX, cinematic.EndX, cinematic.Duration);
                AddCinematicSequence(cinematic.Name, cinematic.StartX, cinematic.EndX,
                    cinematic.Duration, cinematic.AutoMovePlayer, cinematic.PlayerSpeed);
            }

            SetSmoothingFactor(data.Smoothing);
            HMN_CORE_INFO("Camera config loaded: {} points, {} cinematics", data.Points.size(), data.Cinematics.size());
            return true;
        }
        catch (const nlohmann::json::exception& e)
        {
            HMN_CORE_ERROR("CinematicCameraController: JSON error loading '{}': {}", path, e.what());
            return false;
        }
    }

    void CinematicCameraController::TriggerCinematic(const std::string& name, std::function<void()> onEnd)
    {
        for (auto& cinematic : m_Cinematics)
        {
            if (cinematic.Name == name)
            {
                HMN_CORE_INFO("Cinematic started: '{}' (duration={:.1f}s)", name, cinematic.Duration);

                m_InCinematic = true;
                m_ActiveCinematic = &cinematic;
                m_CinematicProgress = 0.0f;
                m_OnCinematicEnd = std::move(onEnd);
                return;
            }
        }

        HMN_CORE_WARN("CinematicCameraController: cinematic '{}' not found", name);
    }

    void CinematicCameraController::EndCinematic()
    {
        if (m_ActiveCinematic)
            HMN_CORE_INFO("Cinematic ended: '{}'", m_ActiveCinematic->Name);

        m_InCinematic = false;
        m_ActiveCinematic = nullptr;
        m_CinematicProgress = 0.0f;
        m_SmoothingFactor = 0.15f;

        // Fire the Lua callback if one was registered
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
            if (cinematic.HasTriggered)
                continue;

            // Trigger when player enters the zone
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

        // Reset HasTriggered so this cinematic can fire again (supports hot-reload)
        for (auto& cinematic : m_Cinematics)
        {
            if (cinematic.Name == name)
            {
                cinematic.HasTriggered = false;
                break;
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
        if (m_CameraEntity && m_CameraEntity.HasComponent<CameraComponent>())
            m_CameraEntity.GetComponent<CameraComponent>().Camera.SetOrthographicSize(m_ZoomLevel);
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