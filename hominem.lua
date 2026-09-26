-- Hominem engine build module (premake).
--
-- Include this from a game's premake5.lua to add the engine + all its vendor
-- projects to that game's workspace:
--     include "external/Hominem/hominem.lua"
--
-- All paths are resolved relative to THIS file (_SCRIPT_DIR), so it works no
-- matter where the engine submodule lives. Build outputs go under the consuming
-- workspace via %{wks.location}. Linking is by project name, so premake wires the
-- vendor .libs automatically regardless of their output dirs.

local HMN = _SCRIPT_DIR

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

HominemRoot = HMN

-- These vendor projects' premake files are owned by the engine (the fork repos
-- don't carry them). Copy each next to its submodule source so the file's relative
-- paths resolve, then include it. Submodules are 'ignore=untracked', so these copies
-- don't show up as dirty. The submodule dirs already exist (the standalone premake5.lua
-- and the game premake5.lua both auto-init submodules before including this module).
local vendorPremake = {
    { "GLFW.lua",           "/Hominem/vendor/GLFW/premake5.lua" },
    { "imgui.lua",          "/Hominem/vendor/imgui/premake5.lua" },
    { "Box2D.lua",          "/Hominem/vendor/Box2D/premake5.lua" },
    { "tracy.lua",          "/Hominem/vendor/tracy/premake5.lua" },
    { "meshoptimizer.lua",  "/Hominem/vendor/meshoptimizer/premake5.lua" },
    { "assimp.lua",         "/Hominem/vendor/assimp/premake5.lua" },
    { "msdf-atlas-gen.lua", "/Hominem/vendor/msdf-atlas-gen/premake5.lua" },
    { "msdfgen.lua",        "/Hominem/vendor/msdf-atlas-gen/msdfgen/premake5.lua" },
}
for _, m in ipairs(vendorPremake) do
    os.copyfile(HMN .. "/premake/vendor/" .. m[1], HMN .. m[2])
end

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

-- NVIDIA DLSS (NGX SDK). The submodule pins an SDK release but is `update = none`: the
-- full repo is ~1.4 GB of binaries for every platform, which a recursive init would pull.
-- Instead it is fetched here as a partial, sparse clone of just the headers, the Windows
-- x64 libs and both nvngx_dlss.dll builds (~30 MB download). Mirrors hmn_sync_dlss in
-- CMakeLists.txt; keep the two in step.
local DLSS = HMN .. "/Hominem/vendor/DLSS"
local DLSSSparsePaths = {
    "/include/",
    "/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib",
    "/lib/Windows_x86_64/x64/nvsdk_ngx_d_dbg.lib",
    "/lib/Windows_x86_64/dev/nvngx_dlss.dll",
    "/lib/Windows_x86_64/rel/nvngx_dlss.dll",
}

local function dlssGit(args)
    local ok = os.execute('git -C "' .. DLSS .. '" ' .. args)
    if ok ~= true and ok ~= 0 then
        print("WARNING: Hominem: `git " .. args .. "` failed in " .. DLSS)
    end
end

local function syncDLSS()
    local pinned = os.outputof('git -C "' .. HMN .. '" ls-files --stage -- Hominem/vendor/DLSS')
    pinned = pinned and pinned:match("^160000 (%x+)")
    if not pinned then return end

    if os.isdir(DLSS .. "/.git") then
        local head = os.outputof('git -C "' .. DLSS .. '" rev-parse HEAD')
        if head == pinned and os.isfile(DLSS .. "/include/nvsdk_ngx.h") then return end
    else
        local url = os.outputof('git -C "' .. HMN .. '" config -f .gitmodules --get submodule.Hominem/vendor/DLSS.url')
        os.mkdir(DLSS)
        dlssGit("init -q")
        dlssGit("remote add origin " .. url)
    end

    print("Hominem: fetching DLSS SDK " .. pinned .. " (headers, Windows x64 libs, DLLs)...")
    dlssGit("fetch -q --depth 1 --filter=blob:none origin " .. pinned)
    dlssGit("sparse-checkout set --no-cone " .. table.concat(DLSSSparsePaths, " "))
    dlssGit("-c advice.detachedHead=false checkout -q --detach " .. pinned)
end
syncDLSS()

local HominemDLSSEnabled = os.isfile(DLSS .. "/include/nvsdk_ngx.h")
if not HominemDLSSEnabled then
    print("WARNING: DLSS SDK not found at " .. DLSS .. "; building without DLSS")
end

group "Dependencies"
    include (HMN .. "/Hominem/vendor/GLFW")
    include (HMN .. "/Hominem/vendor/Glad")
    include (HMN .. "/Hominem/vendor/imgui")
    include (HMN .. "/Hominem/vendor/msdf-atlas-gen")
    include (HMN .. "/Hominem/vendor/assimp")
    include (HMN .. "/Hominem/vendor/Box2D")
    include (HMN .. "/Hominem/vendor/tracy")
    include (HMN .. "/Hominem/vendor/meshoptimizer")
group ""

project "Hominem"
    location  (HMN .. "/build/Hominem")
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

    -- A StaticLib's links are merged into Hominem.lib, so consumers need nothing extra.
    -- NGX loads nvngx_dlss.dll at runtime; during development it is read straight from
    -- the submodule, like engine resources. The dev DLL carries the debug overlay and
    -- must never ship, so only Dist points at the release one.
    if HominemDLSSEnabled then
        includedirs { DLSS .. "/include" }
        libdirs     { DLSS .. "/lib/Windows_x86_64/x64" }
        defines     { "HMN_ENABLE_DLSS" }

        filter "configurations:Debug"
            links   { "nvsdk_ngx_d_dbg" }   -- /MDd
            defines { 'HMN_DLSS_DLL_DIR="' .. DLSS .. '/lib/Windows_x86_64/dev"' }
        filter "configurations:Release"
            links   { "nvsdk_ngx_d" }
            defines { 'HMN_DLSS_DLL_DIR="' .. DLSS .. '/lib/Windows_x86_64/dev"' }
        filter "configurations:Dist"
            links   { "nvsdk_ngx_d" }
            defines { 'HMN_DLSS_DLL_DIR="' .. DLSS .. '/lib/Windows_x86_64/rel"' }
        filter {}
    end

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
