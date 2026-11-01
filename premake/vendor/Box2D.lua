local dir = HominemRoot .. "/Hominem/vendor/Box2D"

project "Box2D"
	location (_MAIN_SCRIPT_DIR .. "/build/vendor/Box2D")
	kind "StaticLib"
	language "C"
	cdialect "C17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		dir .. "/src/**.h",
		dir .. "/src/**.c",
		dir .. "/include/**.h"
	}

	includedirs
	{
		dir .. "/include",
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
        symbols "off"
