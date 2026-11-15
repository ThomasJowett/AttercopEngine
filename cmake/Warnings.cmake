# Warning settings for third-party and engine targets

# MSVC: remove the /W3 CMake puts in the default flags, so each target sets its own level
if (MSVC)
	foreach(flags CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
		string(REGEX REPLACE "/W[0-4]" "" ${flags} "${${flags}}")
	endforeach()
endif()

# Third-party targets: no warnings, no unity build, and their headers count as system headers
function(attercop_third_party_target target)
	get_target_property(type ${target} TYPE)
	if (NOT type STREQUAL "INTERFACE_LIBRARY")
		# Drop the project's own warning level so it doesn't clash with ours
		get_target_property(options ${target} COMPILE_OPTIONS)
		if (options)
			list(FILTER options EXCLUDE REGEX "^(/W[0-4]|-Wall|-Wextra)$")
			set_target_properties(${target} PROPERTIES COMPILE_OPTIONS "${options}")
		endif()
		target_compile_options(${target} PRIVATE $<IF:$<BOOL:${MSVC}>,/W0,-w>)
		set_target_properties(${target} PROPERTIES UNITY_BUILD OFF)
	endif()
	set_property(TARGET ${target} APPEND PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
		$<TARGET_PROPERTY:${target},INTERFACE_INCLUDE_DIRECTORIES>)
endfunction()

# The engine's own targets: warning level, shared definitions and the unity build
function(attercop_target_warnings target)
	if (MSVC)
		target_compile_options(${target} PRIVATE /W3)
		target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
	endif()
	if (ATTERCOP_ENABLE_UNITY_BUILD)
		set_target_properties(${target} PROPERTIES UNITY_BUILD ON)
	endif()
endfunction()
