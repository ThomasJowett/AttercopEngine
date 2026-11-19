cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# Oldest macOS the builds run on, the minimum of the prebuilt wgpu-native library; must be set before project()
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(Attercop)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link options every target built here gets, applied by attercop_apply_common_settings below
set(ATTERCOP_LINK_OPTIONS)
if (EMSCRIPTEN)
    list(APPEND ATTERCOP_LINK_OPTIONS "-sUSE_GLFW=3" "-sUSE_WEBGPU=1" "-sASYNCIFY" "-sALLOW_MEMORY_GROWTH=1" "-sNO_EXIT_RUNTIME=1")
    # Configure a separate build directory with -DCMAKE_BUILD_TYPE=Release for a much smaller, faster web build
    list(APPEND ATTERCOP_LINK_OPTIONS "$<$<CONFIG:Debug>:-sASSERTIONS=1>" "$<$<CONFIG:Release>:-O2>")
endif()

# Applied to the engine's own targets by attercop_target_warnings
option(ATTERCOP_ENABLE_UNITY_BUILD "Enable unity build" OFF)
include(cmake/Warnings.cmake)

# Local fixes for the vendored imgui WebGPU backend that aren't reachable via a submodule version
# bump yet (see the patch file for why - it covers two logically separate fixes, kept in one file
# because they touch adjacent insertion points and splitting them broke the idempotency check below
# on a second configure: reverse-applying just the first fix failed once the second fix's content
# was sitting immediately next to it). Applied here, rather than committed directly into the
# submodule checkout, so it survives fresh clones/CI without anyone needing a manual step.
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/imgui_wgpu_local_fixes.patch
        -DWORKING_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/Engine/vendor/imgui
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch_idempotent.cmake
    RESULT_VARIABLE IMGUI_PATCH_RESULT
)
if (NOT IMGUI_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to apply imgui WebGPU patch")
endif()

# Default to Debug; pass -DCMAKE_BUILD_TYPE=Release for an optimised build (multi-config generators pick per build)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(group_files_by_directory FILE_LIST)
    foreach(file IN LISTS ${FILE_LIST})
        get_filename_component(file_path "${file}" DIRECTORY)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/src" "" group_path "${file_path}")
        string(REPLACE "/" "\\" group_name "${group_path}")
        source_group("${group_name}" FILES "${file}")
    endforeach()
endfunction()

# Include paths and compile options every target built here gets, applied by attercop_apply_common_settings below
set(ATTERCOP_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/Engine/src)
set(ATTERCOP_SYSTEM_INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/Engine/vendor
    ${CMAKE_SOURCE_DIR}/Engine/vendor/spdlog/include
    ${CMAKE_SOURCE_DIR}/Engine/vendor/box2d/include
    ${CMAKE_SOURCE_DIR}/Engine/vendor/lua
    ${CMAKE_SOURCE_DIR}/Engine/vendor/imgui
    ${CMAKE_SOURCE_DIR}/Engine/vendor/stb
    ${CMAKE_SOURCE_DIR}/Engine/vendor/simpleini
    ${CMAKE_SOURCE_DIR}/Engine/vendor/EnTT
    ${CMAKE_SOURCE_DIR}/Engine/vendor/msdf-atlas-gen/msdf-atlas-gen
    ${CMAKE_SOURCE_DIR}/Engine/vendor/msdf-atlas-gen/msdfgen
    ${CMAKE_SOURCE_DIR}/Engine/vendor/GLFW/include
    ${CMAKE_SOURCE_DIR}/Engine/vendor/GLAD/include
)
set(ATTERCOP_COMPILE_OPTIONS)

if (NOT EMSCRIPTEN)
    find_package(OpenGL REQUIRED)
    list(APPEND ATTERCOP_SYSTEM_INCLUDE_DIRECTORIES ${OPENGL_INCLUDE_DIRS})
endif()

# Puts the settings above in front of each built target's own, in this directory and every one below it
function(attercop_apply_common_settings directory)
    get_property(targets DIRECTORY ${directory} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        # Only targets that compile, as directory settings did
        get_target_property(type ${target} TYPE)
        if (type STREQUAL "UTILITY" OR type STREQUAL "INTERFACE_LIBRARY")
            continue()
        endif()
        target_include_directories(${target} SYSTEM BEFORE PRIVATE ${ATTERCOP_SYSTEM_INCLUDE_DIRECTORIES})
        target_include_directories(${target} BEFORE PRIVATE ${ATTERCOP_INCLUDE_DIRECTORIES})
        if (ATTERCOP_COMPILE_OPTIONS)
            target_compile_options(${target} BEFORE PRIVATE ${ATTERCOP_COMPILE_OPTIONS})
        endif()
        if (ATTERCOP_LINK_OPTIONS)
            target_link_options(${target} BEFORE PRIVATE ${ATTERCOP_LINK_OPTIONS})
        endif()
    endforeach()
    get_property(subdirectories DIRECTORY ${directory} PROPERTY SUBDIRECTORIES)
    foreach(subdirectory IN LISTS subdirectories)
        attercop_apply_common_settings(${subdirectory})
    endforeach()
endfunction()


IF(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    set(LINUX TRUE)
ENDIF()

if (MSVC)
    # Edit and Continue (/ZI) and Just My Code in Debug only
    list(APPEND ATTERCOP_COMPILE_OPTIONS /MP $<IF:$<CONFIG:Debug>,/ZI,/Zi> $<$<CONFIG:Debug>:/JMC>)
    set(DEFAULT_STARTUP_TARGET Editor)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${DEFAULT_STARTUP_TARGET})
endif()

set(BOX2D_USER_SETTINGS OFF CACHE INTERNAL "")
set(BOX2D_BUILD_TESTBED OFF CACHE INTERNAL "")
set(BOX2D_BUILD_DOCS OFF CACHE INTERNAL "")
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE INTERNAL "")
# set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(GLFW_INSTALL OFF CACHE INTERNAL "" )
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
if (LINUX)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
elseif (EMSCRIPTEN)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
# set(GLFW_LIBRARY_TYPE OFF CACHE INTERNAL "")
set(tinyxml2_BUILD_TESTING OFF CACHE INTERNAL "")
set(tinyxml2_INSTALL_CMAKEDIR OFF CACHE INTERNAL "")
set(tinyxml2_INSTALL_PKGCONFIGDIR OFF CACHE INTERNAL "")

add_subdirectory(Engine/vendor)
if (NOT EMSCRIPTEN)
    add_subdirectory(Engine/vendor/GLFW)
endif()
add_subdirectory(Engine/vendor/TinyXml2)
add_subdirectory(Engine/vendor/box2d)
add_subdirectory(Engine/vendor/glfw3webgpu)
add_subdirectory(Engine/vendor/tracy)
add_subdirectory(Engine/vendor/sol)
add_subdirectory(Engine/vendor/WebGPU-distribution)

foreach(target glfw tinyxml2 box2d glfw3webgpu TracyClient sol2)
    if (TARGET ${target})
        attercop_third_party_target(${target})
    endif()
endforeach()

add_subdirectory(Engine)
include(cmake/Slang.cmake)
add_subdirectory(Engine/shaders)
add_subdirectory(Editor)
add_subdirectory(Runtime)

option(ATTERCOP_BUILD_SCENE_TESTS "Build SceneTest and UnitTests and register them with CTest" ON)
if (ATTERCOP_BUILD_SCENE_TESTS AND NOT EMSCRIPTEN)
    enable_testing()
    add_subdirectory(Tests)
endif()

attercop_apply_common_settings(${CMAKE_SOURCE_DIR})

set(THIRD_PARTY_FOLDER "ThirdParty")
if (TARGET glfw)
    set_target_properties(glfw PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/GLFW)
endif()
if (TARGET update_mappings)
    set_target_properties(update_mappings PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/GLFW)
endif()
set_target_properties(tinyxml2 PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(box2d PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(freetype PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(glad PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(imgui PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(lua PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(miniz PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(stb PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(msdf-atlas-gen PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/msdfgen)
set_target_properties(msdfgen PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/msdfgen)
set_target_properties(miniaudio PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(TracyClient PROPERTIES FOLDER ${THIRD_PARTY_FOLDER})
set_target_properties(glfw3webgpu PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/GLFW)


