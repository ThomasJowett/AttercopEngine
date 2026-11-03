# Compiles the Slang shaders in Engine/shaders into GLSL and WGSL in the build folder, and copies them into
# each target's data/Shaders. slangc is downloaded from the pinned Slang release, so nothing has to be installed.
#
# Usage, from Engine/shaders/CMakeLists.txt:
#   attercop_add_shader(<name> TARGETS glsl wgsl)
#   attercop_add_shader_target()
# and for every executable that loads shaders:
#   attercop_target_shaders(<target>)

set(SLANGC "" CACHE FILEPATH "slangc to use instead of the downloaded release")

set(ATTERCOP_SLANG_VERSION "2026.20")
set(ATTERCOP_SHADER_SOURCE_DIR "${PROJECT_SOURCE_DIR}/Engine/shaders")
set(ATTERCOP_SHADER_OUTPUT_DIR "${PROJECT_BINARY_DIR}/Shaders")
file(MAKE_DIRECTORY "${ATTERCOP_SHADER_OUTPUT_DIR}")

if (NOT SLANGC)
	set(_slangUrl "https://github.com/shader-slang/slang/releases/download/v${ATTERCOP_SLANG_VERSION}/slang-${ATTERCOP_SLANG_VERSION}")
	if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
		set(_slangArchive "macos-aarch64.tar.gz")
		set(_slangHash "07fa8833fdbb343c3674c84fdd885910f495f3daaaa0c732b32980151e8b58c4")
	elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
		set(_slangArchive "macos-x86_64.tar.gz")
		set(_slangHash "b4f01f615dc625f60c593d7d0a4bbd38e05dfa2a34bfd3973017c309d866feaa")
	elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
		set(_slangArchive "linux-aarch64-glibc-2.28.tar.gz")
		set(_slangHash "9959d741713162efdbd863ff6d32400432b6e103eae3a9ee0afbdba6a00c6a9e")
	elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
		set(_slangArchive "linux-x86_64-glibc-2.27.tar.gz")
		set(_slangHash "b831477e994dee2fb36a3e83c54ad89cbfeefd47b4d973b2d8bed65b64780996")
	elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
		set(_slangArchive "windows-x86_64.zip")
		set(_slangHash "e6dd32bbddf1720ebb4e679d55e9635ce792865a5bd84f3d2cd9af28647c0174")
	endif()

	if (_slangArchive)
		if (POLICY CMP0135)
			cmake_policy(SET CMP0135 NEW)
		endif()
		include(FetchContent)
		FetchContent_Declare(slang
			URL "${_slangUrl}-${_slangArchive}"
			URL_HASH SHA256=${_slangHash}
		)
		FetchContent_MakeAvailable(slang)
		if (CMAKE_HOST_WIN32)
			set(SLANGC "${slang_SOURCE_DIR}/bin/slangc.exe" CACHE FILEPATH "" FORCE)
		else()
			set(SLANGC "${slang_SOURCE_DIR}/bin/slangc" CACHE FILEPATH "" FORCE)
		endif()
	else()
		message(FATAL_ERROR "No Slang release for ${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_PROCESSOR}: set SLANGC to a local slangc")
	endif()
endif()

file(GLOB _shaderModules CONFIGURE_DEPENDS "${ATTERCOP_SHADER_SOURCE_DIR}/Common/*.slang")
list(APPEND _shaderModules "${PROJECT_SOURCE_DIR}/Engine/src/Renderer/ShaderBindings.h")

function(_attercop_compile_shader name output)
	set(source "${ATTERCOP_SHADER_SOURCE_DIR}/${name}.slang")
	set(header "${PROJECT_SOURCE_DIR}/cmake/FinishGeneratedShader.cmake")
	add_custom_command(
		OUTPUT "${ATTERCOP_SHADER_OUTPUT_DIR}/${output}"
		COMMAND "${SLANGC}" "${source}"
			-I "${ATTERCOP_SHADER_SOURCE_DIR}/Common" -I "${PROJECT_SOURCE_DIR}/Engine/src/Renderer"
			-matrix-layout-column-major -line-directive-mode none
			${ARGN}
			-o "${ATTERCOP_SHADER_OUTPUT_DIR}/${output}"
		COMMAND "${CMAKE_COMMAND}" "-DFILE=${ATTERCOP_SHADER_OUTPUT_DIR}/${output}" "-DSOURCE=Engine/shaders/${name}.slang" -P "${header}"
		DEPENDS "${source}" ${_shaderModules} "${header}"
		COMMENT "Compiling ${name}.slang to ${output}"
		VERBATIM
	)
	set_property(GLOBAL APPEND PROPERTY ATTERCOP_SHADER_OUTPUTS "${ATTERCOP_SHADER_OUTPUT_DIR}/${output}")
	set_property(GLOBAL APPEND PROPERTY ATTERCOP_SHADER_SOURCES "${source}")
endfunction()

function(attercop_add_shader name)
	cmake_parse_arguments(ARG "" "" "TARGETS" ${ARGN})
	foreach(target ${ARG_TARGETS})
		if (target STREQUAL "glsl")
			_attercop_compile_shader(${name} "${name}.vert" -target glsl -profile glsl_450 -DTARGET_GLSL -entry vs_main -stage vertex)
			_attercop_compile_shader(${name} "${name}.frag" -target glsl -profile glsl_450 -DTARGET_GLSL -entry fs_main -stage fragment)
		elseif (target STREQUAL "wgsl")
			_attercop_compile_shader(${name} "${name}.wgsl" -target wgsl -DTARGET_WGSL -entry vs_main -entry fs_main)
		else()
			message(FATAL_ERROR "attercop_add_shader(${name}): unknown target '${target}'")
		endif()
	endforeach()
endfunction()

function(attercop_add_shader_target)
	get_property(outputs GLOBAL PROPERTY ATTERCOP_SHADER_OUTPUTS)
	get_property(sources GLOBAL PROPERTY ATTERCOP_SHADER_SOURCES)
	list(REMOVE_DUPLICATES sources)
	add_custom_target(AttercopShaders DEPENDS ${outputs} SOURCES ${sources} ${_shaderModules})
	set_target_properties(AttercopShaders PROPERTIES FOLDER Engine)
	add_dependencies(Engine AttercopShaders)
endfunction()

# Puts the generated shaders in <target>'s data/Shaders: in the bundle on macOS, preloaded on the web,
# otherwise copied next to the executable
function(attercop_target_shaders target)
	get_property(outputs GLOBAL PROPERTY ATTERCOP_SHADER_OUTPUTS)
	set_source_files_properties(${outputs} PROPERTIES GENERATED TRUE)
	add_dependencies(${target} AttercopShaders)
	get_target_property(isBundle ${target} MACOSX_BUNDLE)

	if (EMSCRIPTEN)
		# SHELL: keeps CMake from de-duplicating the second --preload-file
		target_link_options(${target} PRIVATE "SHELL:--preload-file \"${ATTERCOP_SHADER_OUTPUT_DIR}@/data/Shaders\"")
		set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${outputs})
	elseif (APPLE AND isBundle)
		set_source_files_properties(${outputs} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/data/Shaders")
		target_sources(${target} PRIVATE ${outputs})
		source_group("Data/Shaders" FILES ${outputs})
	else()
		# LINK_DEPENDS relinks, and so recopies, when a shader changes (Makefiles and Ninja only)
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/data/Shaders"
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${ATTERCOP_SHADER_OUTPUT_DIR}" "$<TARGET_FILE_DIR:${target}>/data/Shaders"
			COMMENT "Copying shaders to ${target}"
		)
		set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${outputs})
	endif()
endfunction()
