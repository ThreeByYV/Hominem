-- PhysX SDK premake5 configuration
-- This assumes PhysX SDK is installed in this directory

project "PhysX"
    kind "None"  -- Header-only project for includes
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    -- This is just a placeholder project for organization
    -- Actual PhysX libraries are linked directly in main project
