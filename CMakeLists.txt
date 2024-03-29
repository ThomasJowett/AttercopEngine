cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
cmake_policy(VERSION 3.8)
project(
    Attercop
    VERSION 0.1.0
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE "Debug")

add_subdirectory(external/spdlog)

add_subdirectory(App)

set_target_properties(spdlog PROPERTIES FOLDER ThirdParty/spdlog)