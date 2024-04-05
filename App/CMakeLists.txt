file(GLOB APP_SOURCES
    src/*.cpp
)

file(GLOB APP_HEADERS
    ${CMAKE_SOURCE_DIR}/include/*.hpp
)

add_executable(App ${APP_SOURCES} ${APP_HEADERS})

set_target_properties(App PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(App PRIVATE /W4)
else()
    target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()

if(XCODE)
    set_target_properties(App PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()

target_link_libraries(App PRIVATE
    spdlog
    SDL2::SDL2
    webgpu
    sdl2webgpu
)

target_include_directories(App PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_copy_webgpu_binaries(App)