-- Standalone engine workspace. To consume the engine from a game instead, include
-- hominem.lua from the game's own premake5.lua.

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
