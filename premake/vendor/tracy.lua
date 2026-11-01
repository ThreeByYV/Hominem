local dir = HominemRoot .. "/Hominem/vendor/tracy"

project "Tracy"
    location (_MAIN_SCRIPT_DIR .. "/build/vendor/Tracy")
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        dir .. "/public/TracyClient.cpp"
    }

    includedirs
    {
        dir .. "/public"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"
        defines { "TRACY_ENABLE", "TRACY_NO_SYSTEM_TRACING" }

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        defines { "TRACY_ENABLE", "TRACY_NO_SYSTEM_TRACING" }

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"
