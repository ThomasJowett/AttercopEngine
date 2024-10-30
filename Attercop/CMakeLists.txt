file(GLOB ATTERCOP_SOURCES
    src/*.cpp
)

file(GLOB ATTERCOP_HEADERS
    ${CMAKE_SOURCE_DIR}/include/*.hpp
)

add_compile_options("$<$<CONFIG:DEBUG>:-DDEBUG>" "$<$<CONFIG:DEBUG>:-DENABLE_ASSERTS>")

add_library(Attercop ${ATTERCOP_SOURCES} ${ATTERCOP_HEADERS})

set_target_properties(Attercop PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(Attercop PRIVATE /W4)
else()
    target_compile_options(Attercop PRIVATE -Wall -Wextra -pedantic)
endif()

if(XCODE)
    set_target_properties(Attercop PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()

target_link_libraries(Attercop PUBLIC
    spdlog
    SDL2::SDL2
    webgpu
    sdl2webgpu
)

target_include_directories(Attercop PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

install(DIRECTORY ../include/
    DESTINATION include/Attercop
)

install(TARGETS Attercop
    EXPORT AttercopTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/Attercop
)