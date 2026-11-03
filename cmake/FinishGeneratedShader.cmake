# Marks a shader written by slangc as generated, and moves WGSL resource declarations to the top
# in binding order (wgpu's derived layouts mis-bind resources on Metal when they're out of order).
#
# Usage:
#   cmake -DFILE=path/to/output -DSOURCE=Engine/shaders/<name>.slang -P FinishGeneratedShader.cmake

file(READ "${FILE}" content)

if(FILE MATCHES "\\.wgsl$")
	# Semicolons would split the CMake lists below
	string(REPLACE ";" "<semicolon>" content "${content}")
	string(REGEX MATCHALL "@binding\\([0-9]+\\) @group\\([0-9]+\\) var[^\n]*\n\n?" declarations "${content}")

	set(keyed "")
	foreach(declaration IN LISTS declarations)
		string(REPLACE "${declaration}" "" content "${content}")
		string(REGEX MATCH "^@binding\\(([0-9]+)\\) @group\\(([0-9]+)\\)" _ "${declaration}")
		set(binding "000${CMAKE_MATCH_1}")
		set(group "000${CMAKE_MATCH_2}")
		# Zero-padded so the list sorts numerically
		string(REGEX REPLACE "0*([0-9][0-9][0-9][0-9])$" "\\1" binding "${binding}")
		string(REGEX REPLACE "0*([0-9][0-9][0-9][0-9])$" "\\1" group "${group}")
		string(STRIP "${declaration}" declaration)
		list(APPEND keyed "${group}${binding}|${declaration}")
	endforeach()
	list(SORT keyed)

	set(sorted "")
	foreach(entry IN LISTS keyed)
		string(REGEX REPLACE "^[0-9]+\\|" "" entry "${entry}")
		string(APPEND sorted "${entry}\n")
	endforeach()

	if(sorted)
		set(content "${sorted}\n${content}")
	endif()
	string(REPLACE "<semicolon>" ";" content "${content}")
endif()

file(WRITE "${FILE}" "// Generated from ${SOURCE} - do not edit\n${content}")
