project "assimp"
    location (_MAIN_SCRIPT_DIR .. "/build/vendor/assimp")
    kind "Utility"

    -- Headers are referenced directly from hominem.lua via IncludeDir.
    -- hominem.lua builds the lib once at generation via scripts/Build-Assimp-MSVC.bat.
    -- CMake output: vendor/assimp/build/lib/{Debug,Release}/assimp-vc143-mt{d}.lib

    filter "system:windows"
        systemversion "latest"
