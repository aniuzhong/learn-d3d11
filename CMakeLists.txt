cmake_minimum_required(VERSION 3.21)
cmake_policy(VERSION 3.21)

project(LearnD3D11 LANGUAGES CXX)

# ---- C++ standard ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Windows: UNICODE API, avoid min/max macro pollution, UTF-8 source ----
add_compile_definitions(UNICODE _UNICODE NOMINMAX)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---- Default build type ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug, Release, RelWithDebInfo)" FORCE)
endif()

# ---- Search for bundled cmake modules ----
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

# ---- Output directory ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ---- Global include directory (bundled headers: GLM, stb, Framework) ----
include_directories(${CMAKE_SOURCE_DIR}/includes)

# ---- Optional: ASSIMP (Ch3+) ----
find_package(ASSIMP QUIET)
if(ASSIMP_FOUND)
    message(STATUS "Found ASSIMP ${ASSIMP_VERSION} — 3.model_loading enabled")
    add_definitions(-DHAS_ASSIMP)
else()
    message(STATUS "ASSIMP not found — 3.model_loading skipped (install via vcpkg: vcpkg install assimp)")
endif()

# ---- Optional: FreeType (Ch7 text rendering) ----
find_package(Freetype QUIET)
if(Freetype_FOUND)
    message(STATUS "Found FreeType — 7.in_practice/2.text_rendering enabled")
    add_definitions(-DHAS_FREETYPE)
else()
    message(STATUS "FreeType not found — 2.text_rendering skipped")
endif()

# ---- Generation of root_directory.h for resource path resolution ----
configure_file(configuration/root_directory.h.in configuration/root_directory.h)
include_directories(${CMAKE_BINARY_DIR}/configuration)

# =============================================================================
# Static libraries
# =============================================================================

# stb_image — single-header image loader, compiled once
add_library(STB_IMAGE STATIC src/stb_image.cpp)

# Framework — window + D3D11 device + input + timing
add_library(Framework STATIC
    src/framework/Framework.cpp
    includes/framework/Framework.h
    includes/framework/HrCheck.h
)
target_include_directories(Framework PUBLIC ${CMAKE_SOURCE_DIR}/includes)
target_link_libraries(Framework PUBLIC d3d11 dxgi)
# Framework users also get these (PUBLIC means consumers inherit)
target_compile_options(Framework PRIVATE /std:c++20 /W4)

# =============================================================================
# Chapter & demo definitions
# =============================================================================

set(CHAPTERS
    1.getting_started
)

set(1.getting_started
    2.1.hello_triangle
    2.2.hello_triangle_indexed
    2.3.hello_triangle_exercise1
    2.5.hello_triangle_exercise3
    3.1.shaders_cbuffer
    3.2.shaders_interpolation
    3.3.shaders_exercise1
    3.4.shaders_exercise2
)

# Chapters that need ASSIMP
set(ASSIMP_CHAPTERS
    3.model_loading
)

# =============================================================================
# create_demo — generates one executable per tutorial demo
# =============================================================================

function(create_demo chapter demo)
    # Collect all source files for this demo
    file(GLOB SOURCES
        "src/${chapter}/${demo}/*.h"
        "src/${chapter}/${demo}/*.hpp"
        "src/${chapter}/${demo}/*.cpp"
    )

    # Generate target name: "1.getting_started__2.1.hello_triangle"
    if(demo STREQUAL "")
        string(REPLACE "/" "_" target_name ${chapter})
    else()
        set(target_name "${chapter}__${demo}")
    endif()

    add_executable(${target_name} ${SOURCES})

    # Link libraries
    target_link_libraries(${target_name} PRIVATE Framework STB_IMAGE d3d11 dxgi d3dcompiler)
    if(${chapter} IN_LIST ASSIMP_CHAPTERS AND ASSIMP_FOUND)
        target_link_libraries(${target_name} PRIVATE ${ASSIMP_LIBRARY})
        target_compile_definitions(${target_name} PRIVATE HAS_ASSIMP)
    endif()

    # MSVC-specific settings
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /std:c++20 /MP /W4)
        target_link_options(${target_name} PRIVATE /ignore:4099)

        # Each chapter gets its own output subdirectory under bin/
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY              "${CMAKE_SOURCE_DIR}/bin/${chapter}"
            VS_DEBUGGER_WORKING_DIRECTORY         "${CMAKE_SOURCE_DIR}/bin/${chapter}"
        )
    endif()

    # Copy HLSL shader files to output directory (for runtime loading or reference)
    file(GLOB SHADERS
        "src/${chapter}/${demo}/*.hlsl"
        "src/${chapter}/${demo}/*.hlsli"
    )
    foreach(SHADER ${SHADERS})
        add_custom_command(TARGET ${target_name} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SHADER} $<TARGET_FILE_DIR:${target_name}>
            COMMENT "Copying shader: ${SHADER}"
        )
    endforeach()

    # Copy per-demo resources (textures, models) if present
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/${chapter}/${demo}/resources")
        add_custom_command(TARGET ${target_name} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/src/${chapter}/${demo}/resources"
                "$<TARGET_FILE_DIR:${target_name}>/resources"
            COMMENT "Copying resources for ${target_name}"
        )
    endif()

    # Generate VS .user file for debugger settings
    if(MSVC)
        configure_file(
            ${CMAKE_SOURCE_DIR}/configuration/visualstudio.vcxproj.user.in
            ${CMAKE_CURRENT_BINARY_DIR}/${target_name}.vcxproj.user
            @ONLY
        )
    endif()
endfunction()

# =============================================================================
# Generate all demos
# =============================================================================

foreach(CHAPTER ${CHAPTERS})
    foreach(DEMO ${${CHAPTER}})
        create_demo(${CHAPTER} ${DEMO})
    endforeach()
endforeach()

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "=== LearnD3D11 Configuration ===")
message(STATUS "  C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output dir:      ${CMAKE_SOURCE_DIR}/bin")
message(STATUS "  ASSIMP:          ${ASSIMP_FOUND}")
message(STATUS "  FreeType:        ${Freetype_FOUND}")
message(STATUS "================================")
