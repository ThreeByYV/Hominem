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
IncludeDir["GLFW"] = "Hominem/vendor/GLFW/include"
IncludeDir["Glad"] = "Hominem/vendor/Glad/include"
IncludeDir["ImGui"] = "Hominem/vendor/imgui" 

include "Hominem/vendor/GLFW"
include "Hominem/vendor/Glad"
include "Hominem/vendor/imgui"

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
    }

    includedirs
    {
        "%{prj.name}/vendor/spdlog/include",
        "%{prj.name}/src",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}"
    }

    links
    {
        "GLFW",
        "Glad",
        "ImGui",
        "opengl32",
        "gdi32",     
        "user32" 
    }

    filter "files:**/imgui*.cpp"
        flags { "NoPCH" }

    filter "system:windows"
        staticruntime "Off"
        systemversion "latest"
        buildoptions { "/utf-8" } 

        defines
        {         
            "HMN_PLATFORM_WINDOWS",
        }
    
    filter "configurations:Debug"
        defines "HMN_DEBUG"
        symbols "On"
        runtime "Debug"    

    filter "configurations:Release"
        defines "HMN_RELEASE"
        optimize "On"
        runtime "Release"

    filter "configurations:Dist"
        defines "HMN_DIST"
        optimize "On"
        runtime "Release"