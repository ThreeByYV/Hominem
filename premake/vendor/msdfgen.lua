local dir = HominemRoot .. "/Hominem/vendor/msdf-atlas-gen/msdfgen"

project "freetype"
	location (_MAIN_SCRIPT_DIR .. "/build/vendor/freetype")
	kind "StaticLib"
	language "C"
    staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		dir .. "/freetype/include/ft2build.h",
		dir .. "/freetype/include/freetype/*.h",
		dir .. "/freetype/include/freetype/config/*.h",
		dir .. "/freetype/include/freetype/internal/*.h",

		dir .. "/freetype/src/autofit/autofit.c",
		dir .. "/freetype/src/base/ftbase.c",
		dir .. "/freetype/src/base/ftbbox.c",
		dir .. "/freetype/src/base/ftbdf.c",
		dir .. "/freetype/src/base/ftbitmap.c",
		dir .. "/freetype/src/base/ftcid.c",
		dir .. "/freetype/src/base/ftdebug.c",
		dir .. "/freetype/src/base/ftfstype.c",
		dir .. "/freetype/src/base/ftgasp.c",
		dir .. "/freetype/src/base/ftglyph.c",
		dir .. "/freetype/src/base/ftgxval.c",
		dir .. "/freetype/src/base/ftinit.c",
		dir .. "/freetype/src/base/ftmm.c",
		dir .. "/freetype/src/base/ftotval.c",
		dir .. "/freetype/src/base/ftpatent.c",
		dir .. "/freetype/src/base/ftpfr.c",
		dir .. "/freetype/src/base/ftstroke.c",
		dir .. "/freetype/src/base/ftsynth.c",
		dir .. "/freetype/src/base/ftsystem.c",
		dir .. "/freetype/src/base/fttype1.c",
		dir .. "/freetype/src/base/ftwinfnt.c",
		dir .. "/freetype/src/bdf/bdf.c",
		dir .. "/freetype/src/bzip2/ftbzip2.c",
		dir .. "/freetype/src/cache/ftcache.c",
		dir .. "/freetype/src/cff/cff.c",
		dir .. "/freetype/src/cid/type1cid.c",
		dir .. "/freetype/src/gzip/ftgzip.c",
		dir .. "/freetype/src/lzw/ftlzw.c",
		dir .. "/freetype/src/pcf/pcf.c",
		dir .. "/freetype/src/pfr/pfr.c",
		dir .. "/freetype/src/psaux/psaux.c",
		dir .. "/freetype/src/pshinter/pshinter.c",
		dir .. "/freetype/src/psnames/psnames.c",
		dir .. "/freetype/src/raster/raster.c",
		dir .. "/freetype/src/sdf/sdf.c",
		dir .. "/freetype/src/sfnt/sfnt.c",
		dir .. "/freetype/src/smooth/smooth.c",
		dir .. "/freetype/src/truetype/truetype.c",
		dir .. "/freetype/src/type1/type1.c",
		dir .. "/freetype/src/type42/type42.c",
		dir .. "/freetype/src/winfonts/winfnt.c"
	}

	includedirs
	{
		dir .. "/freetype/include"
	}

	defines
	{
		"FT2_BUILD_LIBRARY",
		"_CRT_SECURE_NO_WARNINGS",
		"_CRT_NONSTDC_NO_WARNINGS",
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

project "msdfgen"
	location (_MAIN_SCRIPT_DIR .. "/build/vendor/msdfgen")
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
    staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		dir .. "/core/**.h",
		dir .. "/core/**.hpp",
		dir .. "/core/**.cpp",
		dir .. "/ext/**.h",
		dir .. "/ext/**.hpp",
		dir .. "/ext/**.cpp",
		dir .. "/lib/**.cpp",
		dir .. "/include/**.h"
	}

	includedirs
	{
		dir .. "/include",
		dir .. "/freetype/include"
	}

	defines
	{
		"MSDFGEN_USE_CPP11"
	}

	links
	{
		"freetype"
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