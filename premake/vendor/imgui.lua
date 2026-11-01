local dir = HominemRoot .. "/Hominem/vendor/imgui"

project "ImGui"
    location (_MAIN_SCRIPT_DIR .. "/build/vendor/ImGui")
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        dir .. "/imconfig.h",
        dir .. "/imgui.h",
        dir .. "/imgui.cpp",
        dir .. "/imgui_draw.cpp",
        dir .. "/imgui_internal.h",
        dir .. "/imgui_tables.cpp",
        dir .. "/imgui_widgets.cpp",
        dir .. "/imstb_rectpack.h",
        dir .. "/imstb_textedit.h",
        dir .. "/imstb_truetype.h",
        dir .. "/imgui_demo.cpp",
        dir .. "/backends/imgui_impl_win32.h",
        dir .. "/backends/imgui_impl_win32.cpp",
        dir .. "/backends/imgui_impl_opengl3.h",
        dir .. "/backends/imgui_impl_opengl3.cpp"
    }

    includedirs
    {
        dir
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
        symbols "off"
