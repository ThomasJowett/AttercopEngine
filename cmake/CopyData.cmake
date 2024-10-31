function(target_copy_data target)
    add_custom_command(
        TARGET ${target} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources
        "$<TARGET_FILE_DIR:${target}>/resources"
        COMMENT "Copying the resources folder to '$<TARGET_FILE_DIR:${target}>/resources'..."
    )

    if(NOT EMSCRIPTEN)
        if(SDL_SHARED)
            add_custom_command(
                TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL2>"
                "$<TARGET_FILE_DIR:${target}>"
                COMMENT "Copying '$<TARGET_FILE:SDL2>' to '$<TARGET_FILE_DIR:${target}>'..."
            )
        endif()

        target_copy_webgpu_binaries(${target})
    endif()
endfunction()