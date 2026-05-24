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
IncludeDir["GLFW"]           = "Hominem/vendor/GLFW/include"
IncludeDir["Glad"]           = "Hominem/vendor/Glad/include"
IncludeDir["ImGui"]          = "Hominem/vendor/imgui"
IncludeDir["ImGuiBackends"]  = "Hominem/vendor/imgui/backends"
IncludeDir["glm"]            = "Hominem/vendor/glm"
IncludeDir["stb_image"]      = "Hominem/vendor/stb_image"
IncludeDir["entt"]           = "Hominem/vendor/entt/include"
IncludeDir["msdfgen"]        = "Hominem/vendor/msdf-atlas-gen/msdfgen"
IncludeDir["msdfgen_inc"]    = "Hominem/vendor/msdf-atlas-gen/msdfgen/include"
IncludeDir["msdf_atlas_gen"] = "Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["freetype"]       = "Hominem/vendor/msdf-atlas-gen/msdfgen/freetype/include"
IncludeDir["miniaudio"]      = "Hominem/vendor/miniaudio"
IncludeDir["assimp"]         = "Hominem/vendor/assimp/include"
IncludeDir["assimp_build"]   = "Hominem/vendor/assimp/build/include"
IncludeDir["json"]           = "Hominem/vendor/json"
IncludeDir["Box2D"]          = "Hominem/vendor/Box2D/include"
IncludeDir["tracy"]          = "Hominem/vendor/tracy/public"
IncludeDir["meshoptimizer"]  = "Hominem/vendor/meshoptimizer/src"

include "Hominem/vendor/GLFW"
include "Hominem/vendor/Glad"
include "Hominem/vendor/imgui"
include "Hominem/vendor/msdf-atlas-gen"
include "Hominem/vendor/assimp"
include "Hominem/vendor/Box2D"
include "Hominem/vendor/tracy"
include "Hominem/vendor/meshoptimizer"

VendorIncludes = {
    "Hominem/vendor/spdlog/include",
    "%{IncludeDir.GLFW}",
    "%{IncludeDir.Glad}",
    "%{IncludeDir.ImGui}",
    "%{IncludeDir.ImGuiBackends}",
    "%{IncludeDir.glm}",
    "%{IncludeDir.stb_image}",
    "%{IncludeDir.entt}",
    "%{IncludeDir.msdfgen}",
    "%{IncludeDir.msdfgen_inc}",
    "%{IncludeDir.msdf_atlas_gen}",
    "%{IncludeDir.freetype}",
    "%{IncludeDir.miniaudio}",
    "%{IncludeDir.assimp}",
    "%{IncludeDir.assimp_build}",
    "%{IncludeDir.json}",
    "%{IncludeDir.Box2D}",
    "%{IncludeDir.tracy}",
    "%{IncludeDir.meshoptimizer}"
}

project "Hominem"
    location "Hominem"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
    multiprocessorcompile "on"

    targetdir ("bin/" .. outputdir .. "/Hominem")
    objdir    ("bin-int/" .. outputdir .. "/Hominem")

    pchheader "hmnpch.h"
    pchsource "Hominem/src/Engine/hmnpch.cpp"

    files
    {
        "Hominem/src/Engine/**.h",
        "Hominem/src/Engine/**.cpp",
        "Hominem/src/Engine/**.hpp",
        "Hominem/src/Platform/**.h",
        "Hominem/src/Platform/**.cpp",
        "Hominem/vendor/stb_image/**.h",
        "Hominem/vendor/stb_image/**.cpp",
        "Hominem/vendor/glm/glm/**.hpp",
        "Hominem/vendor/glm/glm/**.inl"
    }

    -- stb_image compiles as plain C without a PCH
    filter "files:**/stb_image/**.cpp"
        enablepch "Off"
    filter {}

    defines { "_CRT_SECURE_NO_WARNINGS", "GLM_ENABLE_EXPERIMENTAL" }

    includedirs
    (
        table.move(VendorIncludes, 1, #VendorIncludes, 3,
            { "Hominem/src/Engine",
              "Hominem/src" })
    )

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

project "PostHominem"
    location "PostHominem"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    multiprocessorcompile "on"

    targetdir ("bin/" .. outputdir .. "/PostHominem")
    objdir    ("bin-int/" .. outputdir .. "/PostHominem")

    debugdir ("bin/" .. outputdir .. "/PostHominem")

    postbuildcommands
    {
        "{COPYDIR} %{prj.location}src/Resources %{cfg.targetdir}/Resources"
    }

    pchheader "hmnpch.h"
    pchsource "PostHominem/src/hmnpch.cpp"

    files
    {
        "PostHominem/src/**.h",
        "PostHominem/src/**.cpp",
        "PostHominem/src/**.hpp",
        "PostHominem/src/Resources/**"
    }

    vpaths
    {
        ["Game/**"]       = "PostHominem/src/Game/**",
        ["Layers/**"]     = "PostHominem/src/Layers/**",
        ["Resources/**"]  = "PostHominem/src/Resources/**",
        ["*"]             = "PostHominem/src/*",
    }

    defines { "_CRT_SECURE_NO_WARNINGS", "GLM_ENABLE_EXPERIMENTAL" }

    includedirs
    (
        table.move(VendorIncludes, 1, #VendorIncludes, 3,
            { "Hominem/src/Engine",
              "PostHominem/src" })
    )

    links
    {
        "Hominem",
        "GLFW",
        "Glad",
        "ImGui",
        "msdf-atlas-gen",
        "msdfgen",
        "freetype",
        "Box2D",
        "Tracy",
        "meshoptimizer"
    }

    filter "files:**/imgui*.cpp"
        enablepch "Off"

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8", "/FS" }
        defines { "HMN_PLATFORM_WINDOWS" }
            removefiles { "Hominem/src/Game/**/MacOS/**" }

    filter "configurations:Debug"
        defines { "HMN_DEBUG", "HMN_ENABLE_ASSERTS", "_DEBUG", "TRACY_ENABLE", "TRACY_NO_SYSTEM_TRACING" }
        symbols "on"
        editandcontinue "Off"
        runtime "Debug"
        linkoptions { "/NODEFAULTLIB:LIBCMTD", "/DEBUG:FASTLINK" }
        libdirs {
            "Hominem/vendor/assimp/build/lib/Debug",
            "Hominem/vendor/assimp/build/contrib/zlib/Debug"
        }
        links { "assimp-vc143-mtd", "zlibstaticd" }

    filter "configurations:Release"
        defines { "HMN_RELEASE", "NDEBUG", "TRACY_ENABLE", "TRACY_NO_SYSTEM_TRACING" }
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
