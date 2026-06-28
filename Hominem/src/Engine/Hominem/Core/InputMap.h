#pragma once

#include <string_view>
#include <unordered_map>
#include <string>

namespace Hominem {

class InputMap
{
public:
    /// Bind a named action to a key code (e.g. HMN_KEY_A).
    static void Bind(std::string_view action, int keycode)
    {
        s_Bindings[std::string(action)] = keycode;
    }

    /// Returns true while the bound key is held. Returns false if action is unbound.
    [[nodiscard]] static bool IsActionPressed(std::string_view action);

    /// Returns the bound key code for an action, or -1 if unbound.
    [[nodiscard]] static int GetKeyCode(std::string_view action);

private:
    static std::unordered_map<std::string, int> s_Bindings;
};

}
