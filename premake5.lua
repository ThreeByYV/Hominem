require "premake/dependencies"

workspace "Hominem"
    architecture "x64"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Setup PhysX
print("\nChecking dependencies...")
Dependencies.SetupPhysX("Hominem/vendor")
print("")

IncludeDir = {}
IncludeDir["GLFW"] = "Hominem/vendor/GLFW/include"
IncludeDir["Glad"] = "Hominem/vendor/Glad/include"
IncludeDir["ImGui"] = "Hominem/vendor/imgui" 
IncludeDir["glm"] = "Hominem/vendor/glm"
IncludeDir["stb_image"] = "Hominem/vendor/stb_image"
IncludeDir["entt"] = "Hominem/vendor/entt/include"
IncludeDir["PhysX"] = "Hominem/vendor/vcpkg/installed/x64-windows/include"
IncludeDir["msdfgen"] = "Hominem/vendor/msdf-atlas-gen/msdfgen"
IncludeDir["msdf_atlas_gen"] = "Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["miniaudio"] = "Hominem/vendor/miniaudio"
IncludeDir["assimp"] = "Hominem/vendor/assimp/include"
IncludeDir["assimp_build"] = "Hominem/vendor/assimp/build/include"
IncludeDir["json"] = "Hominem/vendor/json"

include "Hominem/vendor/GLFW"
include "Hominem/vendor/Glad"
include "Hominem/vendor/imgui"
include "Hominem/vendor/msdf-atlas-gen"
include "Hominem/vendor/assimp"

project "Hominem"
    location "Hominem"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20" 

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

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
        "%{IncludeDir.PhysX}",
        "%{IncludeDir.msdfgen}",
        "%{IncludeDir.msdf_atlas_gen}",
        "%{IncludeDir.miniaudio}",
        "%{IncludeDir.assimp}",
        "%{IncludeDir.assimp_build}",
        "%{IncludeDir.json}"
    }

    links
    {
        "GLFW",
        "Glad",
        "ImGui",
        "msdf-atlas-gen"
    }

    filter "files:**/imgui*.cpp"
        flags { "NoPCH" }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" } 

        defines
        {         
            "HMN_PLATFORM_WINDOWS",
        }
    
    filter "configurations:Debug"
         defines {
            "HMN_DEBUG",
            "HMN_ENABLE_ASSERTS"
        }
        symbols "on"
        runtime "Debug"

        -- Library directories
        libdirs {
            "Hominem/vendor/vcpkg/installed/x64-windows/debug/lib",
            "Hominem/vendor/assimp/build/lib/Debug",
            "Hominem/vendor/assimp/build/contrib/zlib/Debug"
        }

        -- Libraries
        links {
            "PhysXFoundation_64",
            "PhysXCommon_64",
            "PhysX_64",
            "PhysXCooking_64",
            "PhysXExtensions_static_64",
            "PhysXPvdSDK_static_64",
            "assimpd",
            "zlibstaticd"
        }

        -- Copy PhysX DLLs to output
        postbuildcommands {
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/debug/bin/PhysXFoundation_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/debug/bin/PhysXCommon_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/debug/bin/PhysX_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/debug/bin/PhysXCooking_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/debug/bin/PhysXGpu_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/debug/bin/PhysXDevice64.dll" "%{cfg.targetdir}"'
        }

    filter "configurations:Release"
        defines "HMN_RELEASE"
        optimize "on"
        runtime "Release"

        -- Library directories
        libdirs {
            "Hominem/vendor/vcpkg/installed/x64-windows/lib",
            "Hominem/vendor/assimp/build/lib/Release",
            "Hominem/vendor/assimp/build/contrib/zlib/Release"
        }

        -- Libraries
        links {
            "PhysXFoundation_64",
            "PhysXCommon_64",
            "PhysX_64",
            "PhysXCooking_64",
            "PhysXExtensions_static_64",
            "PhysXPvdSDK_static_64",
            "assimp-vc143-mt",
            "zlibstatic"
        }

        -- Copy PhysX DLLs to output
        postbuildcommands {
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXFoundation_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXCommon_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysX_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXCooking_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXGpu_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXDevice64.dll" "%{cfg.targetdir}"'
        }

    filter "configurations:Dist"
        defines "HMN_DIST"
        optimize "On"
        runtime "Release"

        -- Library directories (Dist uses Release)
        libdirs {
            "Hominem/vendor/vcpkg/installed/x64-windows/lib",
            "Hominem/vendor/assimp/build/lib/Release",
            "Hominem/vendor/assimp/build/contrib/zlib/Release"
        }

        -- Libraries
        links {
            "PhysXFoundation_64",
            "PhysXCommon_64",
            "PhysX_64",
            "PhysXCooking_64",
            "PhysXExtensions_static_64",
            "PhysXPvdSDK_static_64",
            "assimp-vc143-mt",
            "zlibstatic"
        }

        -- Copy PhysX DLLs to output
        postbuildcommands {
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXFoundation_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXCommon_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysX_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXCooking_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXGpu_64.dll" "%{cfg.targetdir}"',
            '{COPY} "%{wks.location}/Hominem/vendor/vcpkg/installed/x64-windows/bin/PhysXDevice64.dll" "%{cfg.targetdir}"'
        }