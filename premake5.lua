workspace "Hominem"
    architecture "x64"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"]         = "Hominem/vendor/GLFW/include"
IncludeDir["Glad"]         = "Hominem/vendor/Glad/include"
IncludeDir["ImGui"]        = "Hominem/vendor/imgui"
IncludeDir["glm"]          = "Hominem/vendor/glm"
IncludeDir["stb_image"]    = "Hominem/vendor/stb_image"
IncludeDir["entt"]         = "Hominem/vendor/entt/include"
IncludeDir["msdfgen"]      = "Hominem/vendor/msdf-atlas-gen/msdfgen"
IncludeDir["msdf_atlas_gen"] = "Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["miniaudio"]    = "Hominem/vendor/miniaudio"
IncludeDir["assimp"]       = "Hominem/vendor/assimp/include"
IncludeDir["assimp_build"] = "Hominem/vendor/assimp/build/include"
IncludeDir["json"]         = "Hominem/vendor/json"
IncludeDir["Box2D"]        = "Hominem/vendor/Box2D/include"
IncludeDir["LDtkLoader"]   = "Hominem/vendor/LDtkLoader/include"

include "Hominem/vendor/GLFW"
include "Hominem/vendor/Glad"
include "Hominem/vendor/imgui"
include "Hominem/vendor/msdf-atlas-gen"
include "Hominem/vendor/assimp"
include "Hominem/vendor/Box2D"
include "Hominem/vendor/LDtkLoader"

project "Hominem"
    location "Hominem"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")

    debugdir ("bin/" .. outputdir .. "/%{prj.name}")

    postbuildcommands {
        "{COPYDIR} %{wks.location}Hominem/src/Hominem/Resources %{cfg.targetdir}/Resources"
    }

    pchheader "hmnpch.h"
    pchsource "Hominem/src/hmnpch.cpp"

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/src/**.hpp",
        "%{prj.name}/vendor/stb_image/**.h",
        "%{prj.name}/vendor/stb_image/**.cpp",
        "%{prj.name}/vendor/glm/glm/**.hpp",
        "%{prj.name}/vendor/glm/glm/**.inl"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS",
        "GLM_ENABLE_EXPERIMENTAL"
    }

    includedirs
    {
        "%{prj.name}/vendor/spdlog/include",
        "%{prj.name}/src",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.msdfgen}",
        "%{IncludeDir.msdf_atlas_gen}",
        "%{IncludeDir.miniaudio}",
        "%{IncludeDir.assimp}",
        "%{IncludeDir.assimp_build}",
        "%{IncludeDir.json}",
        "%{IncludeDir.Box2D}",
        "%{IncludeDir.LDtkLoader}"
    }

    links
    {
        "GLFW",
        "Glad",
        "ImGui",
        "msdf-atlas-gen",
        "Box2D",
        "LDtkLoader"
    }

    filter "files:**/imgui*.cpp"
        flags { "NoPCH" }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }
        defines { "HMN_PLATFORM_WINDOWS" }
        removefiles { "%{prj.name}/src/Platform/MacOS/**" }

    filter "configurations:Debug"
        defines { "HMN_DEBUG", "HMN_ENABLE_ASSERTS", "_DEBUG" }
        symbols "on"
        runtime "Debug"
        linkoptions { "/NODEFAULTLIB:LIBCMTD" }
        libdirs {
            "Hominem/vendor/assimp/build/lib/Debug",
            "Hominem/vendor/assimp/build/contrib/zlib/Debug"
        }
        links { "assimp-vc143-mtd", "zlibstaticd" }

    filter "configurations:Release"
        defines { "HMN_RELEASE", "NDEBUG" }
        optimize "on"
        runtime "Release"
        libdirs {
            "Hominem/vendor/assimp/build/lib/Release",
            "Hominem/vendor/assimp/build/contrib/zlib/Release"
        }
        links { "assimp-vc143-mt", "zlibstatic" }

    filter "configurations:Dist"
        defines { "HMN_DIST", "NDEBUG" }
        optimize "On"
        runtime "Release"
        libdirs {
            "Hominem/vendor/assimp/build/lib/Release",
            "Hominem/vendor/assimp/build/contrib/zlib/Release"
        }
        links { "assimp-vc143-mt", "zlibstatic" }
