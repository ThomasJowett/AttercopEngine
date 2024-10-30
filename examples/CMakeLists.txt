add_executable(HelloTriangle ./HelloTriangle.cpp)

target_link_libraries(HelloTriangle PRIVATE Attercop)

target_copy_data(HelloTriangle)

install(TARGETS HelloTriangle
    DESTINATION bin)

if(SDL_SHARED)
    install(TARGETS SDL2 DESTINATION bin)
endif()

install(FILES ${WGPU_RUNTIME_LIB} DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources
    DESTINATION bin
)