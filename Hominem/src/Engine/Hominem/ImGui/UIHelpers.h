#pragma once
// Domain-level UI helpers - each call collapses a repeated block of raw ImGui.
// Include this instead of UI.h when you need to edit engine structs.

#include "UI.h"
#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/Scene.h"          // PostProcessSettings, DirectionalLight
#include "Hominem/Renderer/RenderFrame.h" // Light, LightType
#include "Hominem/Renderer/Renderer3D.h"  // draw-call / triangle counters

#include <glm/gtc/matrix_transform.hpp>

namespace Hominem::UI {

    /// Position + rotation (displayed in degrees) + scale for any Actor.
    inline void EditTransform(Actor& a, float posSpeed = 0.01f)
    {
        DragVec3("Position", a.Position, posSpeed);
        glm::vec3 rotDeg = glm::degrees(a.Rotation);
        if (DragVec3("Rotation (deg)", rotDeg, 0.5f))
            a.Rotation = glm::radians(rotDeg);
        DragVec3("Scale", a.Scale, 0.001f, 0.001f, 100.f);
    }

    /// Full directional light editor (ambient + diffuse + color + direction).
    inline void EditDirectionalLight(DirectionalLight& dl)
    {
        Slider   ("Ambient",       dl.AmbientIntensity, 0.f, 1.f);
        Color3   ("Ambient Color", dl.AmbientColor);
        Slider   ("Diffuse",       dl.DiffuseIntensity, 0.f, 20.f);
        Color3   ("Light Color",   dl.Color);
        SliderVec3("Direction",    dl.Direction, -1.f, 1.f);
    }

    /// All editable fields for one point or spot light.
    inline void EditLight(Light& l)
    {
        DragVec3("Position",      l.Position,    0.05f);
        Color3  ("Color",         l.Color);
        Drag    ("Intensity",     l.Intensity,   0.1f, 0.f, 100.f);
        Drag    ("Radius",        l.Radius,      0.1f, 0.1f, 50.f);
        Drag    ("Source Radius", l.SourceRadius,0.01f, 0.f, 5.f);
        if (l.Type == LightType::Spot)
        {
            DragVec3("Direction",   l.Direction, 0.01f, -1.f, 1.f);
            Drag    ("Inner Angle", l.InnerAngle, 0.5f, 0.f, 89.f);
            Drag    ("Outer Angle", l.OuterAngle, 0.5f, 0.f, 89.f);
        }
    }

    /// Bloom + tone mapping toggles and their sliders.
    inline void EditPostProcess(PostProcessSettings& pp)
    {
        Checkbox("Bloom",        pp.bloomEnabled);
        Checkbox("Tone Mapping", pp.toneMappingEnabled);
        if (pp.bloomEnabled)
        {
            Slider("Bloom Strength",  pp.bloomStrength,  0.f, 3.f);
            Slider("Bloom Threshold", pp.bloomThreshold, 0.f, 2.f);
        }
    }

    /// Top-left perf overlay: FPS (color-coded), draw calls, triangles, physics time.
    inline void PerfPanel(float fps, float frameMs, Scene* scene)
    {
        OverlayWindow("##PerfPanel", {10, 10}, 0.6f, [&] {
            ImVec4 col = fps >= 60.f ? ImVec4(0.2f, 1.f,  0.2f, 1.f)
                       : fps >= 30.f ? ImVec4(1.f,  0.8f, 0.f,  1.f)
                                     : ImVec4(1.f,  0.3f, 0.3f, 1.f);
            ImGui::TextColored(col, "%.0f FPS  %.2f ms", fps, frameMs);

            uint64_t tris  = Renderer3D::GetTriangles();
            const char* u  = tris >= 1000000 ? "M" : tris >= 1000 ? "K" : "";
            float       tv = tris >= 1000000 ? tris / 1000000.f : tris >= 1000 ? tris / 1000.f : (float)tris;
            ImGui::Text("Draw calls  %u",              Renderer3D::GetDrawCalls());
            ImGui::Text("Triangles   %.1f%s",           tv, u);
            ImGui::Text("Groups      %u / %u culled",   Renderer3D::GetGroupsTotal(), Renderer3D::GetGroupsCulled());
            if (scene && scene->GetPhysicsWorld())
                ImGui::Text("Physics     %.2f ms", scene->GetPhysicsWorld()->GetLastStepMs());
        });
    }

    /// Add/remove/select/edit a list of runtime lights. Does not handle saving.
    inline void EditLightList(std::vector<Light>& lights, int& selected)
    {
        ImGui::Text("%d light(s)", (int)lights.size());
        ImGui::SameLine();
        auto addLight = [&](LightType type)
        {
            auto& l    = lights.emplace_back();
            l.Position = (type == LightType::Spot) ? glm::vec3(0.f, 2.f, 0.f) : glm::vec3(0.f, 1.f, 0.f);
            l.Color    = { 1.f, 0.9f, 0.7f };
            l.Type     = type;
            selected   = (int)lights.size() - 1;
        };
        if (SmallButton("Add Point")) addLight(LightType::Point); ImGui::SameLine();
        if (SmallButton("Add Spot"))  addLight(LightType::Spot);

        int removeIdx = -1;
        for (int i = 0; i < (int)lights.size(); i++)
        {
            WithID(i, [&] {
                auto& l = lights[i];
                char label[32];
                snprintf(label, sizeof(label), "%s %d",
                         l.Type == LightType::Spot ? "Spot" : "Point", i);
                if (ImGui::Selectable(label, selected == i))
                    selected = i;
                if (selected == i)
                {
                    EditLight(l);
                    if (SmallButton("Remove")) removeIdx = i;
                }
            });
        }
        if (removeIdx >= 0) { lights.erase(lights.begin() + removeIdx); selected = -1; }
    }

}
