-- Hominem engine build module (premake).
--
-- Include this from a game's premake5.lua to add the engine + all its vendor
-- projects to that game's workspace:
--     include "external/Hominem/hominem.lua"
--
-- All paths are resolved relative to THIS file (_SCRIPT_DIR), so it works no
-- matter where the engine submodule lives. Build outputs go under the consuming
-- workspace via %{wks.location}, and generated project files under its build/
-- folder, so the submodules stay clean. Linking is by project name, so premake
-- wires the vendor .libs automatically regardless of their output dirs.

local HMN = _SCRIPT_DIR
HominemRoot = HMN

-- Walks .gitmodules recursively, so a newly added (or nested) vendor submodule
-- that was never checked out is caught, not just the first one.
local function submodules_missing(dir)
    local gitmodules = dir .. "/.gitmodules"
    if not os.isfile(gitmodules) then return false end
    for sub in io.readfile(gitmodules):gmatch("path%s*=%s*(%S+)") do
        local subdir = dir .. "/" .. sub
        if not (os.isfile(subdir .. "/.git") or os.isdir(subdir .. "/.git")) or submodules_missing(subdir) then
            return true
        end
    end
    return false
end

-- Mirrors the CMake configure-time behavior: init, and if that fails, deep-fetch
-- and force a retry.
if submodules_missing(HMN) then
    print("Hominem: initializing git submodules...")
    local ok = os.execute('git -C "' .. HMN .. '" submodule update --init --recursive')
    if ok ~= true and ok ~= 0 then
        print("Hominem: submodule init incomplete; deep-fetching and retrying...")
        os.execute('git -C "' .. HMN .. '" submodule foreach --recursive git fetch --quiet --tags origin')
        os.execute('git -C "' .. HMN .. '" submodule update --init --recursive --force')
    end
end

-- Global the vendor premake files expect for their targetdir/objdir.
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

VulkanSDK = os.getenv("VULKAN_SDK") or ""
if VulkanSDK == "" then
    print("WARNING: VULKAN_SDK not set; shaderc_combined will not link until the Vulkan SDK is installed")
end

IncludeDir = IncludeDir or {}
IncludeDir["GLFW"]           = HMN .. "/Hominem/vendor/GLFW/include"
IncludeDir["Glad"]           = HMN .. "/Hominem/vendor/Glad/include"
IncludeDir["ImGui"]          = HMN .. "/Hominem/vendor/imgui"
IncludeDir["ImGuiBackends"]  = HMN .. "/Hominem/vendor/imgui/backends"
IncludeDir["glm"]            = HMN .. "/Hominem/vendor/glm"
IncludeDir["stb_image"]      = HMN .. "/Hominem/vendor/stb_image"
IncludeDir["tinyexr"]        = HMN .. "/Hominem/vendor/tinyexr"
IncludeDir["zlib"]           = HMN .. "/Hominem/vendor/assimp/contrib/zlib"
IncludeDir["zlib_build"]     = HMN .. "/Hominem/vendor/assimp/build/contrib/zlib"
IncludeDir["entt"]           = HMN .. "/Hominem/vendor/entt/include"
IncludeDir["msdfgen"]        = HMN .. "/Hominem/vendor/msdf-atlas-gen/msdfgen"
IncludeDir["msdfgen_inc"]    = HMN .. "/Hominem/vendor/msdf-atlas-gen/msdfgen/include"
IncludeDir["msdf_atlas_gen"] = HMN .. "/Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["freetype"]       = HMN .. "/Hominem/vendor/msdf-atlas-gen/msdfgen/freetype/include"
IncludeDir["miniaudio"]      = HMN .. "/Hominem/vendor/miniaudio"
IncludeDir["assimp"]         = HMN .. "/Hominem/vendor/assimp/include"
IncludeDir["assimp_build"]   = HMN .. "/Hominem/vendor/assimp/build/include"
IncludeDir["json"]           = HMN .. "/Hominem/vendor/json"
IncludeDir["Box2D"]          = HMN .. "/Hominem/vendor/Box2D/include"
IncludeDir["tracy"]          = HMN .. "/Hominem/vendor/tracy/public"
IncludeDir["meshoptimizer"]  = HMN .. "/Hominem/vendor/meshoptimizer/src"
IncludeDir["volk"]           = HMN .. "/Hominem/vendor/volk"
IncludeDir["VulkanHeaders"]  = HMN .. "/Hominem/vendor/Vulkan-Headers/include"
IncludeDir["VulkanMemAlloc"] = HMN .. "/Hominem/vendor/VulkanMemoryAllocator/include"
IncludeDir["shaderc"]        = VulkanSDK .. "/Include"

-- Absolute include list every consumer of the engine needs.
HominemVendorIncludes = {
    HMN .. "/Hominem/vendor/spdlog/include",
    IncludeDir.GLFW,
    IncludeDir.Glad,
    IncludeDir.ImGui,
    IncludeDir.ImGuiBackends,
    IncludeDir.glm,
    IncludeDir.stb_image,
    IncludeDir.tinyexr,
    IncludeDir.zlib,
    IncludeDir.zlib_build,
    IncludeDir.entt,
    IncludeDir.msdfgen,
    IncludeDir.msdfgen_inc,
    IncludeDir.msdf_atlas_gen,
    IncludeDir.freetype,
    IncludeDir.miniaudio,
    IncludeDir.assimp,
    IncludeDir.assimp_build,
    IncludeDir.json,
    IncludeDir.Box2D,
    IncludeDir.tracy,
    IncludeDir.meshoptimizer,
    IncludeDir.volk,
    IncludeDir.VulkanHeaders,
    IncludeDir.VulkanMemAlloc,
    IncludeDir.shaderc,
}

-- Engine src roots a consumer needs to compile/PCH against.
HominemEngineIncludes = {
    HMN .. "/Hominem/src/Engine",
    HMN .. "/Hominem/src",
}

-- assimp is a CMake project premake can't build directly, so premake links a prebuilt
-- lib. Build it once here (via the CMake-based script) when the lib is missing, so a
-- fresh `premake5 vs2022` needs no manual assimp step. The script is non-interactive.
if not os.isfile(HMN .. "/Hominem/vendor/assimp/build/lib/Release/assimp.lib") then
    print("Hominem: building assimp (one-time, a few minutes)...")
    local ok = os.execute('"' .. path.translate(HMN .. "/scripts/Build-Assimp-MSVC.bat") .. '"')
    if ok ~= true and ok ~= 0 then
        print("Hominem: assimp build failed. Run scripts/Build-Assimp-MSVC.bat manually")
    end
end

-- The engine owns these vendor premake files, so they live in premake/vendor and
-- root their paths at the submodule explicitly. Any premake5.lua a fork carries is
-- ignored. Glad is in-tree and keeps its own premake5.lua.
group "Dependencies"
    include (HMN .. "/premake/vendor/GLFW.lua")
    include (HMN .. "/Hominem/vendor/Glad")
    include (HMN .. "/premake/vendor/imgui.lua")
    include (HMN .. "/premake/vendor/msdfgen.lua")
    include (HMN .. "/premake/vendor/msdf-atlas-gen.lua")
    include (HMN .. "/premake/vendor/assimp.lua")
    include (HMN .. "/premake/vendor/Box2D.lua")
    include (HMN .. "/premake/vendor/tracy.lua")
    include (HMN .. "/premake/vendor/meshoptimizer.lua")
group ""

project "Hominem"
    location  (_MAIN_SCRIPT_DIR .. "/build/Hominem")
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
    multiprocessorcompile "on"

    targetdir ("%{wks.location}/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Hominem")
    objdir    ("%{wks.location}/bin-int/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Hominem")

    pchheader "hmnpch.h"
    pchsource (HMN .. "/Hominem/src/Engine/hmnpch.cpp")

    files
    {
        HMN .. "/Hominem/src/Engine/**.h",
        HMN .. "/Hominem/src/Engine/**.cpp",
        HMN .. "/Hominem/src/Engine/**.hpp",
        HMN .. "/Hominem/src/Platform/**.h",
        HMN .. "/Hominem/src/Platform/**.cpp",
        HMN .. "/Hominem/vendor/stb_image/**.h",
        HMN .. "/Hominem/vendor/stb_image/**.cpp",
        HMN .. "/Hominem/vendor/tinyexr/tinyexr.h",
        HMN .. "/Hominem/vendor/tinyexr/tinyexr.cpp",
        HMN .. "/Hominem/vendor/glm/glm/**.hpp",
        HMN .. "/Hominem/vendor/glm/glm/**.inl",
        HMN .. "/Hominem/vendor/volk/volk.c",
    }

    filter "files:**/stb_image/**.cpp"
        enablepch "Off"
    filter "files:**/tinyexr/**.cpp"
        enablepch "Off"
    filter "files:**/volk/volk.c"
        enablepch "Off"
    filter "files:**/Vulkan/VulkanMemory.cpp"
        enablepch "Off"
    filter {}

    defines { "_CRT_SECURE_NO_WARNINGS", "GLM_ENABLE_EXPERIMENTAL" }
    defines { 'HMN_ENGINE_RESOURCES_PATH="' .. HMN .. '/Hominem/src/Engine/Resources"' }

    includedirs (table.join(HominemEngineIncludes, HominemVendorIncludes))

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8", "/FS" }
        defines { "HMN_PLATFORM_WINDOWS" }

    filter "configurations:Debug"
        defines { "HMN_DEBUG", "HMN_ENABLE_ASSERTS", "_DEBUG", "TRACY_ENABLE", "TRACY_NO_SYSTEM_TRACING" }
        symbols "on"
        editandcontinue "Off"
        runtime "Debug"

    filter "configurations:Release"
        defines { "HMN_RELEASE", "NDEBUG", "TRACY_ENABLE", "TRACY_NO_SYSTEM_TRACING" }
        optimize "on"
        runtime "Release"

    filter "configurations:Dist"
        defines { "HMN_DIST", "NDEBUG" }
        optimize "On"
        runtime "Release"
