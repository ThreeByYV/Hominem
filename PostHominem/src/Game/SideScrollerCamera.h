#pragma once

#include "Player.h"
#include "Game/WorldConfig.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Scene/SceneCamera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class SideScrollerCamera
{
public:
    struct Config
    {
        float VisibleHeight = 2.0f;   // world units visible vertically — primary zoom knob
        float PlayerScreenY = 0.25f;  // where player sits: 0=bottom, 0.5=center
        float YBias         = 0.f;    // additional world-unit offset: negative = camera lower
        float FOVDeg        = 30.f;
        float XSpeed        = 0.12f;
        float YSpeed        = 0.05f;
        float LeadStrength  = 0.45f;
        float YDeadZone     = 0.15f;

        /// Pulls the overlapping fields out of the on-disk CameraConfig.
        static Config From(const CameraConfig& src)
        {
            Config cfg;
            cfg.VisibleHeight = src.VisibleHeight;
            cfg.PlayerScreenY = src.PlayerScreenY;
            cfg.YBias         = src.YBias;
            cfg.FOVDeg        = src.FOVDeg;
            cfg.XSpeed        = src.XSpeed;
            cfg.YSpeed        = src.YSpeed;
            cfg.LeadStrength  = src.LeadStrength;
            cfg.YDeadZone     = src.YDeadZone;
            return cfg;
        }
    };

    float ComputeCameraZ(float frontFaceZ) const
    {
        return frontFaceZ + CamDist();
    }

    void Init(Hominem::SceneCamera& camera,
              glm::vec3&            cameraPos,
              glm::vec3&            cameraFront,
              float                 aspect,
              float                 cameraZ,
              float                 playerWorldY,
              const Config&         cfg = {})
    {
        m_Cfg    = cfg;
        m_Camera = &camera;
        m_Pos    = &cameraPos;
        m_Aspect = aspect;
        m_CamZ   = cameraZ;
        m_YOffset = m_Cfg.VisibleHeight * (0.5f - m_Cfg.PlayerScreenY);

        cameraFront = { 0.f, 0.f, -1.f };
        cameraPos   = { cameraPos.x, playerWorldY + m_YOffset + m_Cfg.YBias, cameraZ };
        ApplyProjection();
    }

    void    SetTarget(const Player* player) { m_Target = player; }
    Config& GetConfig()                     { return m_Cfg; }
    float   GetCameraZ()             const  { return m_CamZ; }

    void Snap()
    {
        if (!m_Target || !m_Pos) return;
        m_Pos->x = m_Target->Position.x;
        m_Pos->y = m_Target->Position.y + m_YOffset + m_Cfg.YBias;
    }

    void OnUpdate(Hominem::Timestep ts)
    {
        if (!m_Target || !m_Pos) return;

        const float dt      = (float)ts;
        const float smoothX = 1.f - glm::pow(1.f - m_Cfg.XSpeed, dt * 60.f);
        const float smoothY = 1.f - glm::pow(1.f - m_Cfg.YSpeed, dt * 60.f);

        const float targetX = m_Target->Position.x + m_Target->GetVelocity().x * m_Cfg.LeadStrength;
        m_Pos->x = glm::mix(m_Pos->x, targetX, smoothX);

        const float targetY  = m_Target->Position.y + m_YOffset + m_Cfg.YBias;
        const float delta    = targetY - m_Pos->y;
        const float absDelta = glm::abs(delta);
        if (absDelta > m_Cfg.YDeadZone)
            m_Pos->y = glm::mix(m_Pos->y, targetY - glm::sign(delta) * m_Cfg.YDeadZone, smoothY);

        m_Pos->z = m_CamZ;
    }

    void OnWindowResize(float newAspect)
    {
        if (newAspect <= 0.f) return;
        m_Aspect = newAspect;
        ApplyProjection();
    }

private:
    float CamDist() const
    {
        return (m_Cfg.VisibleHeight * 0.5f) / glm::tan(glm::radians(m_Cfg.FOVDeg * 0.5f));
    }

    void ApplyProjection() const
    {
        if (!m_Camera || m_Aspect <= 0.f) return;
        m_Camera->SetPerspective(m_Cfg.FOVDeg, m_Aspect, 0.1f, CamDist() * 2.f + 200.f);
    }

    Config                m_Cfg;
    Hominem::SceneCamera* m_Camera  = nullptr;
    glm::vec3*            m_Pos     = nullptr;
    const Player*         m_Target  = nullptr;
    float                 m_CamZ    = 0.f;
    float                 m_YOffset = 0.f;
    float                 m_Aspect  = 1.f;
};
