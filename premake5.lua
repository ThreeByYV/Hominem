-- Standalone engine workspace. To consume the engine from a game instead, include
-- hominem.lua from the game's own premake5.lua.

-- Auto-init git submodules (with self-heal) before anything tries to include the
-- vendor scripts, mirroring the CMake configure-time behavior. Runs only when the
-- vendor tree looks uninitialized.
local function ensure_submodules(root, sentinel)
    if os.isfile(sentinel) then return end
    print("Hominem: initializing git submodules...")
    local ok = os.execute("git -C \"" .. root .. "\" submodule update --init --recursive")
    if ok ~= true and ok ~= 0 then
        print("Hominem: submodule init incomplete — deep-fetching and retrying...")
        os.execute("git -C \"" .. root .. "\" submodule foreach --recursive git fetch --quiet --tags origin")
        os.execute("git -C \"" .. root .. "\" submodule update --init --recursive --force")
    end
end
ensure_submodules(_SCRIPT_DIR, _SCRIPT_DIR .. "/Hominem/vendor/GLFW/include/GLFW/glfw3.h")

workspace "Hominem"
    architecture "x64"
    startproject "Hominem"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

include "hominem.lua"
