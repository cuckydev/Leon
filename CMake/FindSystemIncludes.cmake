set(FOUND_SYSTEM_INCLUDES "")

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	# Run compiler with /showIncludes
	execute_process(
		COMMAND ${CMAKE_CXX_COMPILER} /c /Fo:nul /showIncludes "${CMAKE_CURRENT_LIST_DIR}/IncludeTest.cpp"
		OUTPUT_VARIABLE COMPILER_OUTPUT
	)

	# Find the first including file
	string(REGEX MATCH "including file:([ ]*)([^\n]*)" FIRST_INCLUDE "${COMPILER_OUTPUT}")
	set(FIRST_INCLUDE "${CMAKE_MATCH_2}")

	# Extract the file path from the matched string
	if (FIRST_INCLUDE)
		string(REGEX REPLACE "([\\\/]+)" "\/" FIRST_INCLUDE "${FIRST_INCLUDE}")
		string(REGEX REPLACE "\/[^\/]+\/?$" "" FIRST_INCLUDE "${FIRST_INCLUDE}")
	else()
		message(FATAL_ERROR "Failed to find first \"including file:\" line through `cl /showIncludes`")
	endif()

	set(FOUND_SYSTEM_INCLUDES "${FIRST_INCLUDE}")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# Run compiler with -v
	execute_process(
		COMMAND ${CMAKE_CXX_COMPILER} -fsyntax-only "${CMAKE_CURRENT_LIST_DIR}/IncludeTest.cpp" -v
		OUTPUT_VARIABLE COMPILER_OUTPUT
		ERROR_VARIABLE COMPILER_ERROR
		COMMAND_ECHO STDOUT
	)

	if (NOT COMPILER_OUTPUT)
		# For some reason, clang is sending the -v output to stderr
		# This is probably subject to change so let's just check if that happened
		set(COMPILER_OUTPUT "${COMPILER_ERROR}")
	endif()

	string(REGEX MATCH "<\.\.\.> search starts here:(.*)End of search list\." INCLUDES "${COMPILER_OUTPUT}")
	set(INCLUDES "${CMAKE_MATCH_1}")

	# Iterate over the resulting list
	STRING(REGEX MATCHALL "[^\n]+" INCLUDES_LINES "${INCLUDES}")

	set(FOUND_SYSTEM_INCLUDES "")
	foreach (LINE ${INCLUDES_LINES})
		string(REGEX REPLACE "([\\\/]+)" "\/" LINE "${LINE}")
		string(REGEX MATCH "([ ]*)(.*)([ ]*)" TRIMMED "${LINE}")
		set(TRIMMED ${CMAKE_MATCH_2})

		if (TRIMMED)
			if (NOT FOUND_SYSTEM_INCLUDES)
				set(FOUND_SYSTEM_INCLUDES "${TRIMMED}")
			else()
				set(FOUND_SYSTEM_INCLUDES "${FOUND_SYSTEM_INCLUDES};${TRIMMED}")
			endif()
		endif()
	endforeach()
else()
	message(FATAL_ERROR "Could not determine your compiler for finding system includes. Try setting LEON_SYSTEM_INCLUDE manually.")
endif()
