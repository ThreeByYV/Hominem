#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

namespace Hominem::UI {

    // Input widgets - return true when value changed

    inline bool Checkbox(const char* label, bool& v)
    { return ImGui::Checkbox(label, &v); }

    inline bool Slider(const char* label, float& v, float min, float max)
    { return ImGui::SliderFloat(label, &v, min, max); }

    inline bool SliderInt(const char* label, int& v, int min, int max)
    { return ImGui::SliderInt(label, &v, min, max); }

    inline bool SliderVec3(const char* label, glm::vec3& v, float min, float max)
    { return ImGui::SliderFloat3(label, &v.x, min, max); }

    inline bool Drag(const char* label, float& v, float speed = 0.01f, float min = 0.f, float max = 0.f)
    { return ImGui::DragFloat(label, &v, speed, min, max); }

    inline bool DragVec2(const char* label, glm::vec2& v, float speed = 0.01f, float min = 0.f, float max = 0.f)
    { return ImGui::DragFloat2(label, &v.x, speed, min, max); }

    inline bool DragVec3(const char* label, glm::vec3& v, float speed = 0.01f, float min = 0.f, float max = 0.f)
    { return ImGui::DragFloat3(label, &v.x, speed, min, max); }

    inline bool Color3(const char* label, glm::vec3& v)
    { return ImGui::ColorEdit3(label, &v.x); }

    inline bool Color4(const char* label, glm::vec4& v)
    { return ImGui::ColorEdit4(label, &v.x); }

    // Buttons - return true when clicked

    inline bool Button(const char* label)
    { return ImGui::Button(label); }

    inline bool SmallButton(const char* label)
    { return ImGui::SmallButton(label); }


    inline void Separator()    { ImGui::Separator(); }
    inline void Text(const char* text)         { ImGui::Text("%s", text); }
    inline void TextDisabled(const char* text) { ImGui::TextDisabled("%s", text); }

    /// Returns true when expanded (wraps ImGui::CollapsingHeader).
    inline bool Collapsible(const char* label, bool defaultOpen = false)
    {
        ImGuiTreeNodeFlags flags = defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0;
        return ImGui::CollapsingHeader(label, flags);
    }

    /// Calls fn only when the header is expanded.
    template<typename Fn>
    inline void Collapsible(const char* label, Fn&& fn, bool defaultOpen = false)
    {
        if (Collapsible(label, defaultOpen)) fn();
    }

    /// Regular window - wraps Begin/End, calls fn only when expanded
    template<typename Fn>
    inline void Window(const char* label, Fn&& fn)
    {
        if (ImGui::Begin(label)) fn();
        ImGui::End();
    }

    /// Fixed overlay window - position/alpha/flags preset, calls fn if visible
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

    // Scoped ID - wraps PushID/PopID so loops never leak
    template<typename Fn>
    inline void WithID(int id, Fn&& fn)
    { ImGui::PushID(id); fn(); ImGui::PopID(); }

    template<typename Fn>
    inline void WithID(const char* id, Fn&& fn)
    { ImGui::PushID(id); fn(); ImGui::PopID(); }

}
