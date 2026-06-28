#include "hmnpch.h"
#include "InputMap.h"
#include "Input.h"

namespace Hominem {

std::unordered_map<std::string, int> InputMap::s_Bindings;

bool InputMap::IsActionPressed(std::string_view action)
{
    auto it = s_Bindings.find(std::string(action));
    if (it == s_Bindings.end()) return false;
    return Input::IsKeyPressed(it->second);
}

int InputMap::GetKeyCode(std::string_view action)
{
    auto it = s_Bindings.find(std::string(action));
    return it == s_Bindings.end() ? -1 : it->second;
}

}
