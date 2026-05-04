#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Renderer/Camera.h"
#include "Hominem/Renderer/CameraController.h"
#include "Hominem/Scene/Entity.h"
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Hominem {

    /**
     * @brief Camera settings at a specific player position
     */
    struct CameraPoint
    {
        float PlayerX;          ///< Player X position
        glm::vec3 CameraOffset; ///< Camera offset from player
        float CameraZoom;       ///< Orthographic zoom level
    };

    /**
     * @brief Cinematic sequence that plays automatically
     */
    struct CinematicSequence
    {
        std::string Name;           ///< Unique identifier for the sequence
        float StartX;               ///< Player X position that triggers the sequence
        float EndX;                 ///< Player X position at end of sequence
        float Duration;             ///< Duration in seconds
        bool AutoMovePlayer;        ///< Whether player auto-walks during sequence
        float PlayerSpeed;          ///< Player movement speed during sequence
        bool HasTriggered = false;  ///< Prevents re-triggering after completion
    };

    /**
     * @brief Camera controller with cinematic sequence support
     *
     * Provides dynamic camera behavior based on player position with support for
     * automatic cinematic sequences. Camera smoothly interpolates between defined
     * camera points, and can trigger scripted sequences at specific locations.
     *
     * Typical usage:
     * 1. Define camera points throughout your level with AddCameraPoint()
     * 2. Add cinematic sequences with AddCinematicSequence()
     * 3. Update camera each frame with UpdateCameraForPlayer() and OnUpdate()
     */
    class CinematicCameraController : public CameraController<OrthographicCamera>
    {
    public:
        /**
         * @brief Constructs a cinematic camera controller
         * @param aspectRatio Screen aspect ratio (width / height)
         */
        CinematicCameraController(float aspectRatio);

        /**
         * @brief Updates camera position and handles cinematic sequences
         * @param ts Time step for this frame
         */
        void OnUpdate(Timestep ts) override;

        /**
         * @brief Handles input events (mouse scroll, window resize)
         * @param e Event to process
         */
        void OnEvent(Event& e) override;

        /**
         * @brief Adds a camera point defining behavior at a specific player position
         *
         * Camera points are interpolated linearly. When the player is between two points,
         * the camera offset and zoom blend smoothly between them.
         *
         * @param playerX Player X position where this camera configuration applies
         * @param offset Camera offset from player (x: right/left, y: up/down, z: forward/back)
         * @param zoom Orthographic zoom (higher = more zoomed out, default = 10.0f)
         *
         * @note Points are automatically sorted by playerX for proper interpolation
         *
         * @example
         * // Normal follow
         * AddCameraPoint(0.0f, { 0.0f, 3.0f, 0.0f }, 8.0f);
         *
         * // Vista - camera high and zoomed out
         * AddCameraPoint(50.0f, { 0.0f, 10.0f, 0.0f }, 18.0f);
         */
        void AddCameraPoint(float playerX, glm::vec3 offset, float zoom = 10.0f);

        /**
         * @brief Updates camera based on current player position
         *
         * Call this each frame to update the camera's target position based on
         * the player's location. Camera will smoothly move toward the target.
         *
         * @param playerPos Current player position (x, y)
         */
        void UpdateCameraForPlayer(glm::vec2 playerPos);

        /**
         * @brief Adds a cinematic sequence that auto-triggers at a location
         *
         * Sequences automatically start when the player reaches startX. During the
         * sequence, player control can be locked and the player can be auto-moved.
         *
         * @param name Unique identifier for this sequence
         * @param startX Player X position that triggers the sequence
         * @param endX Player X position at end of sequence (if auto-moving)
         * @param duration How long the sequence lasts in seconds
         * @param autoMovePlayer If true, player auto-walks from startX to endX
         * @param playerSpeed Movement speed during auto-walk (units per second)
         *
         * @example
         * // Vista reveal that auto-plays
         * AddCinematicSequence("vista", -5.0f, 5.0f, 3.0f, true, 2.0f);
         */
        void AddCinematicSequence(const std::string& name, float startX, float endX,
                                  float duration, bool autoMovePlayer = false, float playerSpeed = 1.0f);

        /**
         * @brief Binds an ECS camera entity — the controller writes position and zoom
         *        directly into its TransformComponent and CameraComponent each frame,
         *        eliminating the need for any external sync.
         * @param entity Entity with CameraComponent and TransformComponent
         */
        void SetCameraEntity(Entity entity) { m_CameraEntity = entity; }

        /**
         * @brief Loads camera points and cinematic sequences from a JSON config file
         * @param path Path to the JSON file
         * @param key  Dot-separated nested key (e.g. "camera_sequences.level_01_vista")
         * @return true if loaded successfully
         */
        bool LoadFromFile(const std::string& path, const std::string& key);

        /**
         * @brief Manually triggers a cinematic sequence by name
         * @param name Name of the sequence to trigger
         */
        void TriggerCinematic(const std::string& name, std::function<void()> onEnd = nullptr);

        /**
         * @brief Registers a callback that fires when a named cinematic completes.
         *        Also resets HasTriggered so the cinematic can fire again (e.g. after hot-reload).
         * @param name Cinematic name matching a loaded CinematicSequence
         * @param cb   Callback to invoke on completion
         */
        void RegisterOnComplete(const std::string& name, std::function<void()> cb);

        /**
         * @brief Manually ends the current cinematic sequence
         *
         * Returns camera to normal follow mode and restores player control.
         */
        void EndCinematic();

        /**
         * @brief Checks if a cinematic sequence is currently playing
         * @return true if in cinematic mode, false if normal gameplay
         */
        bool IsInCinematic() const { return m_InCinematic; }

        /**
         * @brief Gets the current player position
         *
         * During cinematics with auto-movement, this returns the auto-moved position.
         *
         * @return Current player position (x, y)
         */
        glm::vec2 GetPlayerPosition() const { return m_CurrentPlayerPos; }

        /**
         * @brief Sets camera smoothing factor
         *
         * Controls how quickly camera catches up to target position.
         * Lower = smoother but slower, higher = snappier but less smooth.
         *
         * @param factor Smoothing factor (0.0 = no movement, 1.0 = instant, clamped to 0-1)
         */
        void SetSmoothingFactor(float factor) { m_SmoothingFactor = glm::clamp(factor, 0.0f, 1.0f); }

        /**
         * @brief Gets current smoothing factor
         * @return Current smoothing factor
         */
        float GetSmoothingFactor() const { return m_SmoothingFactor; }

        /**
         * @brief Sets orthographic zoom level
         * @param level Zoom level (higher = more zoomed out)
         */
        void SetZoomLevel(float level);

        /**
         * @brief Gets current zoom level
         * @return Current zoom level
         */
        float GetZoomLevel() const { return m_ZoomLevel; }

    private:
        Entity m_CameraEntity;

        std::vector<CameraPoint> m_CameraPoints;
        std::vector<CinematicSequence> m_Cinematics;
        std::unordered_map<std::string, std::function<void()>> m_CinematicCallbacks;

        CinematicSequence* m_ActiveCinematic = nullptr;
        bool m_InCinematic = false;
        float m_CinematicProgress = 0.0f;
        std::function<void()> m_OnCinematicEnd = nullptr;

        float m_SmoothingFactor = 0.15f;
        float m_AspectRatio;
        float m_ZoomLevel = 10.0f;

        glm::vec3 m_TargetPosition = { 0.0f, 0.0f, 0.0f };
        glm::vec2 m_CurrentPlayerPos = { 0.0f, 0.0f };

        /**
         * @brief Interpolates camera settings based on player X position
         * @param playerX Current player X position
         * @return Interpolated camera point
         */
        CameraPoint InterpolateCameraPoint(float playerX) const;

        /**
         * @brief Recalculates camera projection matrix
         */
        void RecalculateProjection();

        /**
         * @brief Checks if player has reached any cinematic trigger points
         */
        void CheckCinematicTriggers();

        /**
         * @brief Handles mouse scroll events for zoom
         * @param e Mouse scroll event
         * @return true if event was handled
         */
        bool OnMouseScrolled(MouseScrolledEvent& e);

        /**
         * @brief Handles window resize events
         * @param e Window resize event
         * @return true if event was handled
         */
        bool OnWindowResized(WindowResizeEvent& e);
    };

    /// Complete camera sequence configuration for JSON loading
    struct CameraSequenceData
    {
        std::vector<CameraPoint> Points;
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