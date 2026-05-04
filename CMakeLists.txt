cmake_minimum_required(VERSION 3.22)
project(HominemWorkspace CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(Hominem/vendor/GLFW)

set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_TESTBED    OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS       OFF CACHE BOOL "" FORCE)
add_subdirectory(Hominem/vendor/Box2D)

add_library(Glad STATIC Hominem/vendor/Glad/src/glad.c)
target_include_directories(Glad PUBLIC Hominem/vendor/Glad/include)

add_library(ImGui STATIC
    Hominem/vendor/imgui/imgui.cpp
    Hominem/vendor/imgui/imgui_draw.cpp
    Hominem/vendor/imgui/imgui_tables.cpp
    Hominem/vendor/imgui/imgui_widgets.cpp
    Hominem/vendor/imgui/backends/imgui_impl_glfw.cpp
    Hominem/vendor/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(ImGui PUBLIC
    Hominem/vendor/imgui
    Hominem/vendor/imgui/backends
)
target_link_libraries(ImGui PRIVATE glfw)

add_library(Tracy STATIC Hominem/vendor/tracy/public/TracyClient.cpp)
target_include_directories(Tracy PUBLIC Hominem/vendor/tracy/public)
target_compile_definitions(Tracy PUBLIC TRACY_ENABLE TRACY_NO_SYSTEM_TRACING)

add_library(assimp_lib STATIC IMPORTED GLOBAL)
set_target_properties(assimp_lib PROPERTIES
    IMPORTED_LOCATION_DEBUG
        "${CMAKE_SOURCE_DIR}/Hominem/vendor/assimp/build/lib/Debug/assimp-vc143-mtd.lib"
    IMPORTED_LOCATION_RELEASE
        "${CMAKE_SOURCE_DIR}/Hominem/vendor/assimp/build/lib/Release/assimp-vc143-mt.lib"
    INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/Hominem/vendor/assimp/include;${CMAKE_SOURCE_DIR}/Hominem/vendor/assimp/build/include"
)

add_library(zlib_lib STATIC IMPORTED GLOBAL)
set_target_properties(zlib_lib PROPERTIES
    IMPORTED_LOCATION_DEBUG
        "${CMAKE_SOURCE_DIR}/Hominem/vendor/assimp/build/contrib/zlib/Debug/zlibstaticd.lib"
    IMPORTED_LOCATION_RELEASE
        "${CMAKE_SOURCE_DIR}/Hominem/vendor/assimp/build/contrib/zlib/Release/zlibstatic.lib"
)

add_library(msdf-atlas-gen STATIC IMPORTED GLOBAL)
set_target_properties(msdf-atlas-gen PROPERTIES
    IMPORTED_LOCATION_DEBUG
        "${CMAKE_SOURCE_DIR}/bin/Debug-windows-x64/msdf-atlas-gen/msdf-atlas-gen.lib"
    IMPORTED_LOCATION_RELEASE
        "${CMAKE_SOURCE_DIR}/bin/Release-windows-x64/msdf-atlas-gen/msdf-atlas-gen.lib"
    INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/Hominem/vendor/msdf-atlas-gen/msdfgen;${CMAKE_SOURCE_DIR}/Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen"
)

set(VENDOR_INCLUDES
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/spdlog/include
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/GLFW/include
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/Glad/include
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/imgui
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/glm
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/stb_image
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/entt/include
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/msdf-atlas-gen/msdfgen
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/miniaudio
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/assimp/include
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/assimp/build/include
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/json
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/Box2D/include
    ${CMAKE_SOURCE_DIR}/Hominem/vendor/tracy/public
)

set(COMMON_DEFINES
    _CRT_SECURE_NO_WARNINGS
    GLM_ENABLE_EXPERIMENTAL
    HMN_PLATFORM_WINDOWS
    $<$<CONFIG:Debug>:HMN_DEBUG;HMN_ENABLE_ASSERTS;_DEBUG;TRACY_ENABLE;TRACY_NO_SYSTEM_TRACING>
    $<$<CONFIG:Release>:HMN_RELEASE;NDEBUG;TRACY_ENABLE;TRACY_NO_SYSTEM_TRACING>
    $<$<CONFIG:Dist>:HMN_DIST;NDEBUG>
)

file(GLOB_RECURSE ENGINE_SOURCES
    "Hominem/src/Engine/*.cpp"
    "Hominem/src/Engine/*.h"
    "Hominem/src/Platform/*.cpp"
    "Hominem/src/Platform/*.h"
    "Hominem/vendor/stb_image/*.h"
    "Hominem/vendor/stb_image/*.cpp"
)

add_library(Hominem STATIC ${ENGINE_SOURCES})

target_include_directories(Hominem PUBLIC
    ${CMAKE_SOURCE_DIR}/Hominem/src/Engine
    ${CMAKE_SOURCE_DIR}/Hominem/src
    ${VENDOR_INCLUDES}
)

target_precompile_headers(Hominem PRIVATE
    "${CMAKE_SOURCE_DIR}/Hominem/src/Engine/hmnpch.h"
)

target_compile_definitions(Hominem PRIVATE ${COMMON_DEFINES})

target_link_libraries(Hominem PUBLIC
    glfw Glad ImGui msdf-atlas-gen box2d Tracy assimp_lib zlib_lib opengl32
)

if(MSVC)
    target_compile_options(Hominem PRIVATE /utf-8 /FS /MP)
endif()

file(GLOB_RECURSE GAME_SOURCES
    "Hominem/src/Game/*.cpp"
    "Hominem/src/Game/*.h"
)

add_executable(PostHominem ${GAME_SOURCES})

target_include_directories(PostHominem PRIVATE
    ${CMAKE_SOURCE_DIR}/Hominem/src/Game
)

target_precompile_headers(PostHominem PRIVATE
    "${CMAKE_SOURCE_DIR}/Hominem/src/Engine/hmnpch.h"
)

target_compile_definitions(PostHominem PRIVATE ${COMMON_DEFINES})

target_link_libraries(PostHominem PRIVATE Hominem)

if(MSVC)
    target_compile_options(PostHominem PRIVATE /utf-8 /FS /MP)
    target_link_options(PostHominem PRIVATE
        $<$<CONFIG:Debug>:/NODEFAULTLIB:LIBCMTD;/DEBUG:FASTLINK>
    )
endif()

add_custom_command(TARGET PostHominem POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Hominem/src/Game/Resources"
        "$<TARGET_FILE_DIR:PostHominem>/Resources"
)
