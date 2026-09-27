#pragma once

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/Frame/RenderFrame.h"
#include "Hominem/Renderer/Frame/RenderSettings.h"
#include "Hominem/Renderer/ForwardPlusRenderer.h"

namespace Hominem::UI {

template<typename Fn>
inline void Window(const char* label, Fn&& fn)
{ if (ImGui::Begin(label)) fn(); ImGui::End(); }

template<typename Fn>
inline void OverlayWindow(const char* id, ImVec2 pos, float bgAlpha, Fn&& fn)
{
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(bgAlpha);
    if (ImGui::Begin(id, nullptr, kFlags)) fn();
    ImGui::End();
}

inline void EditTransform(Actor& a, float posSpeed = 0.01f)
{
    ImGui::DragFloat3("Position", &a.Position.x, posSpeed);
    glm::vec3 rotDeg = glm::degrees(a.Rotation);
    if (ImGui::DragFloat3("Rotation (deg)", &rotDeg.x, 0.5f))
        a.Rotation = glm::radians(rotDeg);
    ImGui::DragFloat3("Scale", &a.Scale.x, 0.001f, 0.001f, 100.f);
}

inline void EditDirectionalLight(DirectionalLight& dl)
{
    ImGui::SliderFloat ("Ambient",       &dl.AmbientIntensity, 0.f, 1.f);
    ImGui::ColorEdit3  ("Ambient Color",  &dl.AmbientColor.x);
    ImGui::SliderFloat ("Diffuse",       &dl.DiffuseIntensity, 0.f, 20.f);
    ImGui::ColorEdit3  ("Light Color",    &dl.Color.x);
    ImGui::SliderFloat3("Direction",      &dl.Direction.x, -1.f, 1.f);
}

inline void EditLight(Light& l)
{
    ImGui::DragFloat3("Position",      &l.Position.x,    0.05f);
    ImGui::ColorEdit3("Color",         &l.Color.x);
    ImGui::DragFloat ("Intensity",     &l.Intensity,     0.1f, 0.f, 100.f);
    ImGui::DragFloat ("Radius",        &l.Radius,        0.1f, 0.1f, 50.f);
    ImGui::DragFloat ("Source Radius", &l.SourceRadius,  0.01f, 0.f, 5.f);
    if (l.Type == LightType::Spot)
    {
        ImGui::DragFloat3("Direction",   &l.Direction.x,  0.01f, -1.f, 1.f);
        ImGui::DragFloat ("Inner Angle", &l.InnerAngle,   0.5f, 0.f, 89.f);
        ImGui::DragFloat ("Outer Angle", &l.OuterAngle,   0.5f, 0.f, 89.f);
    }
}

inline void EditPostProcess(PostProcessSettings& pp)
{
    ImGui::Checkbox   ("Bloom",        &pp.bloomEnabled);
    ImGui::Checkbox   ("Tone Mapping", &pp.toneMappingEnabled);
    if (pp.bloomEnabled)
    {
        ImGui::SliderFloat("Bloom Strength",  &pp.bloomStrength,  0.f, 3.f);
        ImGui::SliderFloat("Bloom Threshold", &pp.bloomThreshold, 0.f, 2.f);
    }
}

/// Every setting in the RenderSettings table, built from the table itself so a new setting
/// needs no widget written for it. The hand-tuned panels stay where the ranges and grouping
/// matter; this is the catch-all for the rest.
inline void EditRenderSettings(const char* path = "render.ini")
{
    if (ImGui::SmallButton("Save##rset"))   RenderSettings::SaveTo(path);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reload##rset")) RenderSettings::LoadFrom(path);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset##rset"))  RenderSettings::ResetToDefaults();

    using Setting = RenderSettings::Setting;

    for (const Setting& s : RenderSettings::Enumerate())
    {
        // Greying the untouched ones makes what you have changed findable in a long list.
        if (s.isDefault)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::BeginDisabled(s.isDerived);

        switch (s.type)
        {
            case Setting::Type::Bool:
                ImGui::Checkbox (s.name, static_cast<bool*>(s.value));         break;
            case Setting::Type::Int:
                ImGui::DragInt  (s.name, static_cast<int*>(s.value));          break;
            case Setting::Type::Float:
                ImGui::DragFloat(s.name, static_cast<float*>(s.value), 0.01f); break;
            case Setting::Type::Choice:
            {
                int count = 0;
                while (s.choices[count]) count++;
                ImGui::Combo(s.name, static_cast<int*>(s.value), s.choices, count);
                break;
            }
        }

        ImGui::EndDisabled();
        if (s.isDefault) ImGui::PopStyleColor();
    }
}

inline void PerfPanel(float fps, float frameMs, Scene* scene)
{
    OverlayWindow("##PerfPanel", {10, 10}, 0.6f, [&] {
        ImVec4 col = fps >= 60.f ? ImVec4(0.2f, 1.f,  0.2f, 1.f)
                   : fps >= 30.f ? ImVec4(1.f,  0.8f, 0.f,  1.f)
                                 : ImVec4(1.f,  0.3f, 0.3f, 1.f);
        ImGui::TextColored(col, "%.0f FPS  %.2f ms", fps, frameMs);

        uint64_t tris = ForwardPlusRenderer::GetTriangles();
        const char* u = tris >= 1000000 ? "M" : tris >= 1000 ? "K" : "";
        float      tv = tris >= 1000000 ? tris / 1000000.f : tris >= 1000 ? tris / 1000.f : (float)tris;
        ImGui::Text("Draw calls  %u",            ForwardPlusRenderer::GetDrawCalls());
        ImGui::Text("Triangles   %.1f%s",         tv, u);
        ImGui::Text("Groups      %u / %u culled", ForwardPlusRenderer::GetGroupsTotal(), ForwardPlusRenderer::GetGroupsCulled());
        if (scene && scene->GetPhysicsWorld())
            ImGui::Text("Physics     %.2f ms", scene->GetPhysicsWorld()->GetLastStepMs());
    });
}

inline void EditLightList(std::vector<Light>& lights, int& selected)
{
    ImGui::Text("%d light(s)", (int)lights.size());
    ImGui::SameLine();
    auto addLight = [&](LightType type) {
        auto& l    = lights.emplace_back();
        l.Position = (type == LightType::Spot) ? glm::vec3(0.f, 2.f, 0.f) : glm::vec3(0.f, 1.f, 0.f);
        l.Color    = { 1.f, 0.9f, 0.7f };
        l.Type     = type;
        selected   = (int)lights.size() - 1;
    };
    if (ImGui::SmallButton("Add Point")) addLight(LightType::Point); ImGui::SameLine();
    if (ImGui::SmallButton("Add Spot"))  addLight(LightType::Spot);

    int removeIdx = -1;
    for (int i = 0; i < (int)lights.size(); i++)
    {
        ImGui::PushID(i);
        char label[32];
        snprintf(label, sizeof(label), "%s %d",
                 lights[i].Type == LightType::Spot ? "Spot" : "Point", i);
        if (ImGui::Selectable(label, selected == i)) selected = i;
        if (selected == i)
        {
            EditLight(lights[i]);
            if (ImGui::SmallButton("Remove")) removeIdx = i;
        }
        ImGui::PopID();
    }
    if (removeIdx >= 0) { lights.erase(lights.begin() + removeIdx); selected = -1; }
}

}
