#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Events/MouseEvent.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Renderer/Camera.h"
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Hominem {

    struct CameraPoint
    {
        float PlayerX;
        glm::vec3 CameraOffset;
        float CameraZoom;
    };

    struct CinematicSequence
    {
        std::string Name;
        float StartX;
        float EndX;
        float Duration;
        bool AutoMovePlayer;
        float PlayerSpeed;
        bool HasTriggered = false;
    };

    class CinematicCameraController : public RefCounted
    {
    public:
        explicit CinematicCameraController(float aspectRatio);

        /// Ported from SideScrollerCamera — projected-focus lookahead (Keren, "Scroll Back").
        float LeadStrength = 0.f;
        /// Ported from SideScrollerCamera — camera-window dead zone on Y only (Keren, "Scroll Back").
        float YDeadZone    = 0.f;

        /// One-call setup: binds camera/position/front, sets ortho projection + viewport,
        /// places the camera at initialPos, and applies feel parameters. Call once from
        /// OnSceneReady — mirrors the old SideScrollerCamera::Init single-call shape.
        void Init(Camera& camera, glm::vec3& cameraPos, glm::vec3& cameraFront,
                  uint32_t viewportW, uint32_t viewportH, const glm::vec3& initialPos,
                  float visibleHeight, float leadStrength, float yDeadZone, float smoothing,
                  float yBias = 0.f);

        void OnUpdate(Timestep ts);
        void OnEvent(Event& e);

        void AddCameraPoint(float playerX, glm::vec3 offset, float zoom = 10.0f);
        /// velocity is used for the lead/lookahead offset — pass Player::GetVelocity().
        void UpdateCameraForPlayer(glm::vec2 playerPos, glm::vec2 velocity = { 0.f, 0.f });

        /// Used when fewer than 2 CameraPoints are authored, so continuous follow
        /// still works with zero JSON content (region-based-anchors needs >=2 to interpolate).
        void SetDefaultFraming(glm::vec3 offset, float zoom) { m_DefaultOffset = offset; m_DefaultZoom = zoom; }

        /// Instant cut to the current target — no smoothing. For skip-intro / level-load snaps.
        void Snap();

        void AddCinematicSequence(const std::string& name, float startX, float endX,
                                  float duration, bool autoMovePlayer = false, float playerSpeed = 1.0f);

        /// Bind the scene's camera and position so the controller writes to them each frame.
        void SetCameraTarget(Camera* camera, glm::vec3* position)
        {
            m_SceneCamera    = camera;
            m_CameraPosition = position;
        }

        bool LoadFromFile(const std::string& path, const std::string& key);

        void TriggerCinematic(const std::string& name, std::function<void()> onEnd = nullptr);
        void RegisterOnComplete(const std::string& name, std::function<void()> cb);
        void EndCinematic();

        bool       IsInCinematic()  const { return m_InCinematic; }
        glm::vec2  GetPlayerPosition() const { return m_CurrentPlayerPos; }

        void  SetSmoothingFactor(float factor) { m_SmoothingFactor = glm::clamp(factor, 0.0f, 1.0f); }
        float GetSmoothingFactor() const { return m_SmoothingFactor; }

        void  SetZoomLevel(float level);
        float GetZoomLevel() const { return m_ZoomLevel; }

        /// Vertical offset applied to the default (region-less) camera target — shifts
        /// framing so the player isn't dead-center. Negative moves the frame's focus down,
        /// showing more headroom above the player.
        void  SetYBias(float bias) { m_DefaultOffset.y = bias; }
        float GetYBias() const { return m_DefaultOffset.y; }

    private:
        // Non-owning pointers into the Scene's camera state.
        Camera*      m_SceneCamera    = nullptr;
        glm::vec3*   m_CameraPosition = nullptr;

        std::vector<CameraPoint>      m_CameraPoints;
        std::vector<CinematicSequence> m_Cinematics;
        std::unordered_map<std::string, std::function<void()>> m_CinematicCallbacks;

        CinematicSequence*    m_ActiveCinematic  = nullptr;
        bool                  m_InCinematic      = false;
        float                 m_CinematicProgress = 0.0f;
        std::function<void()> m_OnCinematicEnd   = nullptr;

        float     m_SmoothingFactor = 0.15f;
        float     m_AspectRatio;
        float     m_ZoomLevel = 10.0f;

        glm::vec3 m_TargetPosition    = { 0.0f, 0.0f, 0.0f };
        glm::vec2 m_CurrentPlayerPos  = { 0.0f, 0.0f };

        glm::vec3 m_DefaultOffset = { 0.f, 0.f, -30.f };
        float     m_DefaultZoom   = 10.0f;

        CameraPoint InterpolateCameraPoint(float playerX) const;
        void RecalculateProjection();
        void CheckCinematicTriggers();
        bool OnMouseScrolled(MouseScrolledEvent& e);
        bool OnWindowResized(WindowResizeEvent& e);
    };

    struct CameraSequenceData
    {
        std::vector<CameraPoint>      Points;
        std::vector<CinematicSequence> Cinematics;
        float Smoothing = 0.15f;
    };

}

namespace nlohmann {

    inline void from_json(const json& j, glm::vec3& v) { v.x = j.at("x"); v.y = j.at("y"); v.z = j.at("z"); }

    inline void from_json(const json& j, Hominem::CameraPoint& p)
    {
        j.at("PlayerX").get_to(p.PlayerX);
        j.at("CameraOffset").get_to(p.CameraOffset);
        j.at("CameraZoom").get_to(p.CameraZoom);
    }

    inline void from_json(const json& j, Hominem::CinematicSequence& s)
    {
        j.at("Name").get_to(s.Name);
        j.at("StartX").get_to(s.StartX);
        j.at("EndX").get_to(s.EndX);
        j.at("Duration").get_to(s.Duration);
        j.at("AutoMovePlayer").get_to(s.AutoMovePlayer);
        j.at("PlayerSpeed").get_to(s.PlayerSpeed);
    }

    inline void from_json(const json& j, Hominem::CameraSequenceData& d)
    {
        j.at("Points").get_to(d.Points);
        j.at("Cinematics").get_to(d.Cinematics);
        j.at("Smoothing").get_to(d.Smoothing);
    }

}
