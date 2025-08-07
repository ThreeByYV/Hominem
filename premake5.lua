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


include "Hominem/vendor/GLFW"
include "Hominem/vendor/Glad"

project "Hominem"
    location "Hominem"
    kind "ConsoleApp"
    language "C++"

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
         "%{IncludeDir.Glad}"
    }

    links
    {
        "GLFW",
        "Glad",
        "opengl32.lib"
    }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "Off"
        systemversion "latest"
         buildoptions { "/utf-8" } 

        defines
        {         
            "HMN_PLATFORM_WINDOWS",
        }

        -- postbuildcommands
        -- {
        --     {"{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox"}
        -- }
    
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

    -- filter { "system:windows", "configurations:Release" }
    --     buildoptions "/MT"

