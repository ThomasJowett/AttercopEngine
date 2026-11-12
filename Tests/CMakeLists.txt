add_compile_options("$<$<CONFIG:DEBUG>:-DDEBUG>" "$<$<CONFIG:DEBUG>:-DENABLE_ASSERTS>")

file(GLOB SCENE_TEST_SOURCES
    SceneTest/*.cpp
    SceneTest/*.h
)

file(GLOB_RECURSE DATA_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Editor/data/*")

add_executable(SceneTest ${SCENE_TEST_SOURCES} ${DATA_FILES})
target_link_libraries(SceneTest PRIVATE Engine)
attercop_target_shaders(SceneTest)
target_copy_webgpu_binaries(SceneTest)

add_custom_command(
        TARGET SceneTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SceneTest>/data"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Editor/data"
            "$<TARGET_FILE_DIR:SceneTest>/data"
        COMMENT "Copying data folder to binary dir"
    )

if (MSVC)
    target_compile_options(SceneTest PRIVATE "/utf-8")
endif()

# Every scene in Scenes/Scenes ending in Test.scene is one test; see design/headless-mode-and-ci-tests.md
file(GLOB TEST_SCENES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Scenes/Scenes/*Test.scene)
foreach(scene ${TEST_SCENES})
    get_filename_component(name ${scene} NAME_WE)
    add_test(NAME ${name}
        COMMAND SceneTest --project ${CMAKE_CURRENT_SOURCE_DIR}/Scenes/Tests.proj --scene ${scene}
            --frames 6000 --timeout 30 --require-verdict --log ${CMAKE_CURRENT_BINARY_DIR}/logs/${name}.txt)
    set_tests_properties(${name} PROPERTIES TIMEOUT 60 LABELS scene)
endforeach()

# Unit tests for engine code that doesn't need a window or a scene
file(GLOB UNIT_TEST_SOURCES CONFIGURE_DEPENDS
    UnitTests/*.cpp
    UnitTests/*.h
)

add_executable(UnitTests ${UNIT_TEST_SOURCES})
# main.cpp holds doctest's implementation, compiled outside unity batches
set_source_files_properties(UnitTests/main.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
if (NOT MSVC)
    # Silences doctest's sprintf deprecation warnings
    set_source_files_properties(UnitTests/main.cpp PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
endif()
target_link_libraries(UnitTests PRIVATE Engine)
target_compile_definitions(UnitTests PRIVATE ATTERCOP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_copy_webgpu_binaries(UnitTests)

if (MSVC)
    target_compile_options(UnitTests PRIVATE "/utf-8")
endif()

add_test(NAME UnitTests COMMAND UnitTests)
set_tests_properties(UnitTests PROPERTIES TIMEOUT 60 LABELS unit)
