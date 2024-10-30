add_executable(CoreTests ./CoreTests.cpp)

target_link_libraries(CoreTests PRIVATE
    Attercop
)

target_copy_webgpu_binaries(CoreTests)

add_test(NAME CoreTests COMMAND CoreTests)