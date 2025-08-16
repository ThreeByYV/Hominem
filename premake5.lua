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
IncludeDir["glm"] = "Hominem/vendor/glm" 
IncludeDir["stb_image"] = "Hominem/vendor/stb_image" 


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
        "%{prj.name}/vendor/stb_image/**.h",
        "%{prj.name}/vendor/stb_image/**.cpp",
        "%{prj.name}/vendor/glm/glm/**.hpp",
        "%{prj.name}/vendor/glm/glm/**.inl"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

    includedirs
    {
        "%{prj.name}/vendor/spdlog/include",
        "%{prj.name}/src",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb_image}"
    }

    links
    {
        "GLFW",
        "Glad",
        "ImGui"
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

    filter "configurations:Release"
        defines "HMN_RELEASE"
        optimize "on"
        runtime "Release"

    filter "configurations:Dist"
        defines "HMN_DIST"
        optimize "On"
        runtime "Release"