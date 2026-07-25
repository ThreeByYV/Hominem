cmake_minimum_required(VERSION 3.22)

# Forced /MD + IDL=0 everywhere: shaderc/assimp only ship as Release libs here.
# TODO: build them from source in matching Debug config, then remove this.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

project(Hominem CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)

# Auto-init git submodules at configure time so a plain clone (e.g. opened
# straight into CLion) works without a manual `git submodule update --init`.
# Runs only when the vendor tree looks uninitialized, so reconfigures stay fast.
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git"
        AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/GLFW/CMakeLists.txt")
    message(STATUS "Hominem: initializing git submodules...")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE _hmn_submod_result
    )
    if(NOT _hmn_submod_result EQUAL 0)
        # Self-heal: an interrupted earlier init can leave a submodule as a partial
        # clone whose pinned commit was never fetched ("unable to find current
        # revision"). Fetch every submodule fully, then retry the checkout.
        message(STATUS "Hominem: submodule init incomplete — deep-fetching and retrying...")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule foreach --recursive
                    ${GIT_EXECUTABLE} fetch --quiet --tags origin
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive --force
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE _hmn_submod_result2
        )
        if(NOT _hmn_submod_result2 EQUAL 0)
            message(WARNING "Hominem: submodules still incomplete. A partial clone is "
                "corrupt; reset it with:\n"
                "  git submodule deinit -f --all && git submodule update --init --recursive")
        endif()
    endif()
endif()

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

# Use lld if available — much faster than GNU ld on large debug builds.
if(NOT MSVC)
    find_program(_LLD lld HINTS "C:/Program Files/LLVM/bin")
    if(_LLD)
        message(STATUS "Using lld: ${_LLD}")
        add_link_options(-fuse-ld=lld)
    endif()
endif()

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
# PRIVATE so TRACY_ENABLE doesn't leak to consumers via PUBLIC interface.
# Each target opts in explicitly via its own compile definitions below.
target_compile_definitions(Tracy PRIVATE TRACY_ENABLE TRACY_NO_SYSTEM_TRACING)

# Pre-create assimp generated-include dirs so CMake configure succeeds before the
# first assimp build (scripts/Build-Assimp.bat will populate them).
if(MSVC)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/include")
    set(_ASSIMP_INC_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/include")
    set(_ASSIMP_ZLIB_INC
        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/contrib/zlib")
else()
    # MinGW/Ninja: single-config generator — separate Debug & Release build dirs.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Debug/include")
    set(_ASSIMP_INC_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Debug/include")
    set(_ASSIMP_ZLIB_INC
        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Debug/contrib/zlib")
endif()

add_library(assimp_lib STATIC IMPORTED GLOBAL)
if(MSVC)
    set_target_properties(assimp_lib PROPERTIES
        IMPORTED_LOCATION_DEBUG          "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/lib/Release/assimp.lib"
        IMPORTED_LOCATION_RELEASE        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/lib/Release/assimp.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/lib/Release/assimp.lib"
        IMPORTED_LOCATION_MINSIZEREL     "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/lib/Release/assimp.lib"
        INTERFACE_INCLUDE_DIRECTORIES    "${_ASSIMP_INC_DIRS}"
    )
else()
    set_target_properties(assimp_lib PROPERTIES
        IMPORTED_LOCATION_DEBUG          "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Debug/lib/libassimpd.a"
        IMPORTED_LOCATION_RELEASE        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Release/lib/libassimp.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Release/lib/libassimp.a"
        IMPORTED_LOCATION_MINSIZEREL     "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Release/lib/libassimp.a"
        INTERFACE_INCLUDE_DIRECTORIES    "${_ASSIMP_INC_DIRS}"
    )
endif()

add_library(zlib_lib STATIC IMPORTED GLOBAL)
if(MSVC)
    set_target_properties(zlib_lib PROPERTIES
        IMPORTED_LOCATION_DEBUG          "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/contrib/zlib/Release/zlibstatic.lib"
        IMPORTED_LOCATION_RELEASE        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/contrib/zlib/Release/zlibstatic.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/contrib/zlib/Release/zlibstatic.lib"
        IMPORTED_LOCATION_MINSIZEREL     "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/contrib/zlib/Release/zlibstatic.lib"
    )
else()
    set_target_properties(zlib_lib PROPERTIES
        IMPORTED_LOCATION_DEBUG          "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Debug/contrib/zlib/libzlibstaticd.a"
        IMPORTED_LOCATION_RELEASE        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Release/contrib/zlib/libzlibstatic.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Release/contrib/zlib/libzlibstatic.a"
        IMPORTED_LOCATION_MINSIZEREL     "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/build/Release/contrib/zlib/libzlibstatic.a"
    )
endif()

if(WIN32)
    set(_VK_SDK_ROOT "")
    if(DEFINED ENV{VULKAN_SDK})
        set(_VK_SDK_ROOT "$ENV{VULKAN_SDK}")
    else()
        file(GLOB _VK_SDK_CANDIDATES "C:/VulkanSDK/*")
        list(SORT _VK_SDK_CANDIDATES ORDER DESCENDING)
        foreach(_candidate IN LISTS _VK_SDK_CANDIDATES)
            if(IS_DIRECTORY "${_candidate}" AND EXISTS "${_candidate}/Lib/shaderc_combined.lib")
                set(_VK_SDK_ROOT "${_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(_VK_SDK_ROOT AND EXISTS "${_VK_SDK_ROOT}/Lib/shaderc_combined.lib")
        message(STATUS "Vulkan SDK: ${_VK_SDK_ROOT}")
        add_library(shaderc_lib STATIC IMPORTED GLOBAL)
        set_target_properties(shaderc_lib PROPERTIES
            IMPORTED_LOCATION_DEBUG          "${_VK_SDK_ROOT}/Lib/shaderc_combined.lib"
            IMPORTED_LOCATION_RELEASE        "${_VK_SDK_ROOT}/Lib/shaderc_combined.lib"
            IMPORTED_LOCATION_RELWITHDEBINFO "${_VK_SDK_ROOT}/Lib/shaderc_combined.lib"
            IMPORTED_LOCATION_MINSIZEREL     "${_VK_SDK_ROOT}/Lib/shaderc_combined.lib"
            INTERFACE_INCLUDE_DIRECTORIES    "${_VK_SDK_ROOT}/Include"
        )
    else()
        message(WARNING "Vulkan SDK not found — shaderc will not be available")
    endif()
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/GLFW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/Glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/stb_image
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/tinyexr
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/entt/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/msdf-atlas-gen/msdfgen
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/msdf-atlas-gen/msdf-atlas-gen
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/miniaudio
    ${_ASSIMP_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/json
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/Box2D/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/tracy/public
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/meshoptimizer/src
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/volk
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/Vulkan-Headers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/VulkanMemoryAllocator/include
)

# Vendor libs compiled without debug info — we never step into them in the debugger
# and -g0 significantly reduces the data GNU ld has to process at link time.
if(NOT MSVC)
    foreach(_vend Glad ImGui Tracy stb_image tinyexr msdfgen msdf-atlas-gen meshoptimizer freetype glfw box2d)
        if(TARGET ${_vend})
            target_compile_options(${_vend} PRIVATE -g0)
        endif()
    endforeach()
endif()

set(COMMON_DEFINES
    _CRT_SECURE_NO_WARNINGS
    GLM_ENABLE_EXPERIMENTAL
    HMN_PLATFORM_WINDOWS
    $<$<CONFIG:Debug>:HMN_DEBUG;HMN_ENABLE_ASSERTS;TRACY_ENABLE;TRACY_NO_SYSTEM_TRACING>
    $<$<CONFIG:Release>:HMN_RELEASE;NDEBUG;TRACY_ENABLE;TRACY_NO_SYSTEM_TRACING>
    $<$<CONFIG:Dist>:HMN_DIST;NDEBUG>
)

add_subdirectory(Hominem/vendor/meshoptimizer)

add_library(Vulkan-Headers INTERFACE)
target_include_directories(Vulkan-Headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/Vulkan-Headers/include
)

add_library(volk STATIC Hominem/vendor/volk/volk.c)
target_include_directories(volk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/volk)
target_link_libraries(volk PUBLIC Vulkan-Headers)
if(WIN32)
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
endif()

add_library(stb_image STATIC Hominem/vendor/stb_image/stb_image.cpp)
target_include_directories(stb_image PUBLIC Hominem/vendor/stb_image)
set_target_properties(stb_image PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
target_compile_options(stb_image PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Y->)

# tinyexr — HDR/EXR loader (stb_image-style single header). Uses the project's
# existing zlib (assimp's zlibstatic) via TINYEXR_USE_MINIZ=0 in tinyexr.cpp.
add_library(tinyexr STATIC Hominem/vendor/tinyexr/tinyexr.cpp)
target_include_directories(tinyexr PUBLIC Hominem/vendor/tinyexr)
target_include_directories(tinyexr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/vendor/assimp/contrib/zlib
    ${_ASSIMP_ZLIB_INC}
)
target_link_libraries(tinyexr PRIVATE zlib_lib)
set_target_properties(tinyexr PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
target_compile_options(tinyexr PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Y->)

file(GLOB_RECURSE ENGINE_SOURCES
    "Hominem/src/Engine/*.cpp"
    "Hominem/src/Engine/*.h"
    "Hominem/src/Platform/*.cpp"
    "Hominem/src/Platform/*.h"
)

add_library(Hominem STATIC ${ENGINE_SOURCES})

target_include_directories(Hominem
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/src/Engine
        ${VENDOR_INCLUDES}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Hominem/src
)

target_precompile_headers(Hominem PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/src/Engine/hmnpch.h"
)

target_compile_definitions(Hominem PRIVATE ${COMMON_DEFINES})

set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/src/Platform/Vulkan/VulkanMemory.cpp"
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

target_link_libraries(Hominem PUBLIC
    glfw Glad ImGui stb_image tinyexr msdf-atlas-gen box2d Tracy assimp_lib zlib_lib opengl32 meshoptimizer volk
)

if(TARGET shaderc_lib)
    target_link_libraries(Hominem PUBLIC shaderc_lib)
endif()

if(WIN32)
    target_link_libraries(Hominem PUBLIC winmm ws2_32 dbghelp)
endif()

if(MSVC)
    target_compile_options(Hominem PRIVATE /utf-8 /FS /MP /we4834)
endif()

# Always read engine resources directly from source during development so
# hot-reload (R key) works without a post-build copy. Remove for distribution builds.
target_compile_definitions(Hominem PRIVATE
    HMN_ENGINE_RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Hominem/src/Engine/Resources"
)

# Standalone target to sync engine resources into a consumer's build dir without a
# full rebuild. Run via: cmake --build <build-dir> --target sync-engine-resources
add_custom_target(sync-engine-resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Hominem/src/Engine/Resources"
        "${CMAKE_BINARY_DIR}/EngineResources"
    COMMENT "Syncing engine resources to build directory"
)
