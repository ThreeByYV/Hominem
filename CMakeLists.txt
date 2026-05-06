cmake_minimum_required(VERSION 3.22)
project(HominemWorkspace CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Block msys64/MinGW header contamination ───────────────────────────────────
# When msys64 is on the system PATH, CMake's find_package() resolves ZLIB/PNG/
# BZip2 to msys64 libraries and injects -IC:\msys64\mingw64\include into every
# target that links them (including freetype). That path is full of GCC-specific
# headers that break MSVC with C2059 errors.
#
# Layer 1 — stop find_* from scanning PATH/system environment entirely.
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF)
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH       OFF)
# Disable pkg-config (it also points at msys64 on Windows with msys64 in PATH).
set(PKG_CONFIG_EXECUTABLE "" CACHE FILEPATH "" FORCE)

# Layer 2 — scrub any msys/mingw dirs from an already-configured target.
function(strip_mingw_includes target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()
    foreach(_prop INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(_dirs ${target_name} ${_prop})
        if(_dirs)
            list(FILTER _dirs EXCLUDE REGEX "[Mm]sys|[Mm]ing[Ww]")
            set_target_properties(${target_name} PROPERTIES ${_prop} "${_dirs}")
        endif()
    endforeach()
endfunction()

# Layer 3 — strip from CMake's own implicit lists so they never reach cl.exe.
if(MSVC)
    foreach(_var
            CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES
            CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES
            CMAKE_SYSTEM_INCLUDE_PATH)
        list(FILTER ${_var} EXCLUDE REGEX "[Mm]sys|[Mm]ing[Ww]")
    endforeach()
endif()
# ─────────────────────────────────────────────────────────────────────────────

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

file(GLOB IMGUI_SOURCES "Hominem/vendor/imgui/*.cpp")
add_library(ImGui STATIC
    ${IMGUI_SOURCES}
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

set(FREETYPE_WITH_ZLIB     OFF CACHE BOOL "" FORCE)
set(FREETYPE_WITH_BZIP2    OFF CACHE BOOL "" FORCE)
set(FREETYPE_WITH_PNG      OFF CACHE BOOL "" FORCE)
set(FREETYPE_WITH_HARFBUZZ OFF CACHE BOOL "" FORCE)
add_subdirectory(Hominem/vendor/freetype)
# Freetype's public headers contain #pragma comment(lib, "freetype.lib") on MSVC.
# The CMake Debug build names the output "freetyped.lib" by default, which the
# linker can't find. Remove the debug postfix so the lib is always "freetype.lib".
set_target_properties(freetype PROPERTIES DEBUG_POSTFIX "")
strip_mingw_includes(freetype)

file(GLOB MSDFGEN_SOURCES
    "Hominem/vendor/msdf-atlas-gen/msdfgen/core/*.cpp"
    "Hominem/vendor/msdf-atlas-gen/msdfgen/ext/*.cpp"
)
add_library(msdfgen STATIC ${MSDFGEN_SOURCES})
target_include_directories(msdfgen PUBLIC
    Hominem/vendor/msdf-atlas-gen/msdfgen
    Hominem/vendor/msdf-atlas-gen/msdfgen/include
)
target_link_libraries(msdfgen PUBLIC freetype)

file(GLOB MSDF_ATLAS_SOURCES "Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen/*.cpp")
add_library(msdf-atlas-gen STATIC ${MSDF_ATLAS_SOURCES})
target_include_directories(msdf-atlas-gen PUBLIC
    Hominem/vendor/msdf-atlas-gen/msdfgen
    Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen
)
target_link_libraries(msdf-atlas-gen PUBLIC msdfgen)

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

add_library(stb_image STATIC Hominem/vendor/stb_image/stb_image.cpp)
target_include_directories(stb_image PUBLIC Hominem/vendor/stb_image)
set_target_properties(stb_image PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
target_compile_options(stb_image PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Y->)

file(GLOB_RECURSE ENGINE_SOURCES
    "Hominem/src/Engine/*.cpp"
    "Hominem/src/Engine/*.h"
    "Hominem/src/Platform/*.cpp"
    "Hominem/src/Platform/*.h"
)

add_library(Hominem STATIC ${ENGINE_SOURCES})

target_include_directories(Hominem
    PUBLIC
        ${CMAKE_SOURCE_DIR}/Hominem/src/Engine
        ${VENDOR_INCLUDES}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Hominem/src
)

target_precompile_headers(Hominem PRIVATE
    "${CMAKE_SOURCE_DIR}/Hominem/src/Engine/hmnpch.h"
)

target_compile_definitions(Hominem PRIVATE ${COMMON_DEFINES})

target_link_libraries(Hominem PUBLIC
    glfw Glad ImGui stb_image msdf-atlas-gen box2d Tracy assimp_lib zlib_lib opengl32
)

if(MSVC)
    target_compile_options(Hominem PRIVATE /utf-8 /FS /MP)
endif()

file(GLOB_RECURSE GAME_SOURCES
    "PostHominem/src/*.cpp"
    "PostHominem/src/*.h"
)

add_executable(PostHominem ${GAME_SOURCES})

target_include_directories(PostHominem PRIVATE
    ${CMAKE_SOURCE_DIR}/PostHominem/src
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
        "${CMAKE_SOURCE_DIR}/PostHominem/src/Resources"
        "$<TARGET_FILE_DIR:PostHominem>/Resources"
)
