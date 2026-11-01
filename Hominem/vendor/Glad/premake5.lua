project "Glad"
	location (_MAIN_SCRIPT_DIR .. "/build/vendor/Glad")
	kind "StaticLib"
	language "C"
	staticruntime "off"
	warnings "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"include/glad/glad.h",
		"include/KHR/khrplatform.h",
		"src/glad.c",
	}

	includedirs
	{
		"include"
	}

	filter "system:windows"
		systemversion "latest"
		staticruntime "On"

	filter { "system:windows", "configurations:Release" }	
		buildoptions "/MT"

	    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        staticruntime "off"

    filter "configurations:Release" 
        runtime "Release"
        staticruntime "off"

    filter "configurations:Dist"
        runtime "Release" 
        staticruntime "off"
