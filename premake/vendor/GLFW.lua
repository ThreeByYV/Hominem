local dir = HominemRoot .. "/Hominem/vendor/GLFW"

project "GLFW"
	location (_MAIN_SCRIPT_DIR .. "/build/vendor/GLFW")
	kind "StaticLib"
	language "C"
	staticruntime "off"
	warnings "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		dir .. "/include/GLFW/glfw3.h",
		dir .. "/include/GLFW/glfw3native.h",
		dir .. "/src/glfw_config.h",
		dir .. "/src/context.c",
		dir .. "/src/init.c",
		dir .. "/src/input.c",
		dir .. "/src/monitor.c",

		dir .. "/src/null_init.c",
		dir .. "/src/null_joystick.c",
		dir .. "/src/null_monitor.c",
		dir .. "/src/null_window.c",

		dir .. "/src/platform.c",
		dir .. "/src/vulkan.c",
		dir .. "/src/window.c",
	}

	filter "system:linux"
		pic "On"

		systemversion "latest"

		files
		{
			dir .. "/src/x11_init.c",
			dir .. "/src/x11_monitor.c",
			dir .. "/src/x11_window.c",
			dir .. "/src/xkb_unicode.c",
			dir .. "/src/posix_module.c",
			dir .. "/src/posix_time.c",
			dir .. "/src/posix_thread.c",
			dir .. "/src/posix_module.c",
			dir .. "/src/glx_context.c",
			dir .. "/src/egl_context.c",
			dir .. "/src/osmesa_context.c",
			dir .. "/src/linux_joystick.c"
		}

		defines
		{
			"_GLFW_X11"
		}

	filter "system:macosx"
		pic "On"

		files
		{
			dir .. "/src/cocoa_init.m",
			dir .. "/src/cocoa_monitor.m",
			dir .. "/src/cocoa_window.m",
			dir .. "/src/cocoa_joystick.m",
			dir .. "/src/cocoa_time.c",
			dir .. "/src/nsgl_context.m",
			dir .. "/src/posix_thread.c",
			dir .. "/src/posix_module.c",
			dir .. "/src/osmesa_context.c",
			dir .. "/src/egl_context.c"
		}

		defines
		{
			"_GLFW_COCOA"
		}

	filter "system:windows"
		systemversion "latest"

		files
		{
			dir .. "/src/win32_init.c",
			dir .. "/src/win32_joystick.c",
			dir .. "/src/win32_module.c",
			dir .. "/src/win32_monitor.c",
			dir .. "/src/win32_time.c",
			dir .. "/src/win32_thread.c",
			dir .. "/src/win32_window.c",
			dir .. "/src/wgl_context.c",
			dir .. "/src/egl_context.c",
			dir .. "/src/osmesa_context.c"
		}

		defines
		{
			"_GLFW_WIN32",
			"_CRT_SECURE_NO_WARNINGS"
		}

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter { "system:windows", "configurations:Debug-AS" }
		runtime "Debug"
		symbols "on"
		sanitize { "Address" }
		flags { "NoRuntimeChecks", "NoIncrementalLink" }

	filter "configurations:Release"
		runtime "Release"
		optimize "speed"

    filter "configurations:Dist"
		runtime "Release"
		optimize "speed"
        symbols "off"
