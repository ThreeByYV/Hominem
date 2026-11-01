local dir = HominemRoot .. "/Hominem/vendor/meshoptimizer"

project "meshoptimizer"
    location (_MAIN_SCRIPT_DIR .. "/build/vendor/meshoptimizer")
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        dir .. "/src/**.cpp",
        dir .. "/src/**.h"
    }

    includedirs
    {
        dir .. "/src"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"
