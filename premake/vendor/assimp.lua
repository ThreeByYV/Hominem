project "assimp"
    kind "Utility"

    -- Headers are referenced directly from the root premake5.lua via IncludeDir.
    -- Build this library first by running scripts/Build-Assimp.bat (one-time setup).
    -- CMake output: vendor/assimp/build/lib/{Debug,Release}/assimp-vc143-mt{d}.lib

    filter "system:windows"
        systemversion "latest"
