project "Tracy"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "public/TracyClient.cpp"
    }

    includedirs
    {
        "public"
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
