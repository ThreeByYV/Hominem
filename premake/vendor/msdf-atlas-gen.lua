local dir = HominemRoot .. "/Hominem/vendor/msdf-atlas-gen"

project "msdf-atlas-gen"
	location (_MAIN_SCRIPT_DIR .. "/build/vendor/msdf-atlas-gen")
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
    staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		dir .. "/msdf-atlas-gen/**.h",
    	dir .. "/msdf-atlas-gen/**.hpp",
    	dir .. "/msdf-atlas-gen/**.cpp"
	}

	includedirs
	{
		dir .. "/msdf-atlas-gen",
		dir .. "/msdfgen",
		dir .. "/msdfgen/include"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}

	links
	{
		"msdfgen"
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
