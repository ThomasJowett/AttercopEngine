cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
cmake_policy(VERSION 3.10)
cmake_policy(SET CMP0077 NEW)

project(
    Attercop
    VERSION 0.1.0
    LANGUAGES CXX C
)

enable_testing()

include(${CMAKE_SOURCE_DIR}/cmake/CopyData.cmake)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE "Debug")

include(FetchContent)

FetchContent_Declare(
  webgpu
  GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
  GIT_TAG        main
)
FetchContent_MakeAvailable(webgpu)

if (MSVC)
    add_compile_options(/MP /ZI /JMC)
    set(DEFAULT_STARTUP_TARGET App)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${DEFAULT_STARTUP_TARGET})
endif()

option(SDL_SHARED OFF)

add_subdirectory(external/spdlog)
add_subdirectory(external/SDL)
add_subdirectory(external/sdl2webgpu)

add_subdirectory(Attercop)
add_subdirectory(tests)
add_subdirectory(examples)

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation Directory" FORCE)

set_target_properties(spdlog PROPERTIES FOLDER ThirdParty/spdlog)