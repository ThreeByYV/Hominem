cmake_minimum_required(VERSION 3.20)
project(Hominem)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB_RECURSE SOURCES
        "Hominem/src/*.cpp"
        "Hominem/src/*.h"
        "Hominem/vendor/stb_image/*.cpp"
        "Hominem/vendor/stb_image/*.h"
)

add_executable(Hominem ${SOURCES})

# Include directories
target_include_directories(Hominem PRIVATE
        Hominem/vendor/spdlog/include
        Hominem/src
        Hominem/vendor/GLFW/include
        Hominem/vendor/Glad/include
        Hominem/vendor/imgui
        Hominem/vendor/glm
        Hominem/vendor/stb_image
        Hominem/vendor/assimp/include
        Hominem/vendor/assimp/build/include
        Hominem/vendor/msdf-atlas-gen/msdfgen
        Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen
)

# Point to the pre-built libraries from Premake/VS build
target_link_directories(Hominem PRIVATE
        ${CMAKE_SOURCE_DIR}/bin/Debug-windows-x64/GLFW
        ${CMAKE_SOURCE_DIR}/bin/Debug-windows-x64/Glad
        ${CMAKE_SOURCE_DIR}/bin/Debug-windows-x64/ImGui
        ${CMAKE_SOURCE_DIR}/Hominem/vendor/assimp/build/lib/Debug
        ${CMAKE_SOURCE_DIR}/bin/Debug-windows-x64/msdf-atlas-gen
)

# Link the pre-built .lib files
target_link_libraries(Hominem PRIVATE
        GLFW.lib
        Glad.lib
        ImGui.lib
        assimp-vc143-mtd.lib
        msdf-atlas-gen.lib
        opengl32
)

# Definitions
target_compile_definitions(Hominem PRIVATE
        _CRT_SECURE_NO_WARNINGS
        HMN_PLATFORM_WINDOWS
        HMN_DEBUG
        HMN_ENABLE_ASSERTS
)

# Precompiled headers
target_precompile_headers(Hominem PRIVATE Hominem/src/hmnpch.h)