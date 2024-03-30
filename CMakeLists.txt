cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
cmake_policy(VERSION 3.8)
project(
    Attercop
    VERSION 0.1.0
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE "Debug")

if (MSVC)
    add_compile_options(/MP /ZI /JMC)
    set(DEFAULT_STARTUP_TARGET App)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${DEFAULT_STARTUP_TARGET})
endif()

add_subdirectory(external/spdlog)
add_subdirectory(external/SDL)
# add_subdirectory(external/sdl2webgpu)

add_subdirectory(App)

set_target_properties(spdlog PROPERTIES FOLDER ThirdParty/spdlog)