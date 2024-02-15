cmake_minimum_required(VERSION 3.26)

# Leon project
project(Leon
	LANGUAGES C CXX
	VERSION 0.2.0
	HOMEPAGE_URL "https://ckdev.org/"
)

# Settings
option(LEON_BUILD_TESTS "Build tests" ON)

# Compile Leon interface
add_library(Leon INTERFACE)

target_include_directories(Leon INTERFACE "Include")

target_compile_definitions(Leon INTERFACE LEON_VERSION="${CMAKE_PROJECT_VERSION}")

# Compile Leon CLI app
add_executable(Leon.CLI
	"Source/Leon.cpp"
	"Source/Parse.cpp"
	"Source/Parse.h"
	"Source/Process.cpp"
	"Source/Process.h"
)

target_link_libraries(Leon.CLI PRIVATE Leon)

# Determine compiler system include directory
set(LEON_SYSTEM_INCLUDES "" CACHE STRING "System header include directories.")

if (NOT LEON_SYSTEM_INCLUDE)
	message(STATUS "Auto-detecting LEON_SYSTEM_INCLUDES")

	include("${CMAKE_CURRENT_LIST_DIR}/CMake/FindSystemIncludes.cmake")
	set(LEON_SYSTEM_INCLUDES "${FOUND_SYSTEM_INCLUDES}")
endif()

# Write system include frame header
# CMake doesn't let us send lists over as pre-processor defines, so let's do this instead
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/SystemIncludeFrame")

set(LEON_SYSTEM_INCLUDE_FRAME "")

foreach (HEADER ${LEON_SYSTEM_INCLUDES})
	set(LEON_SYSTEM_INCLUDE_FRAME "${LEON_SYSTEM_INCLUDE_FRAME}LEON_SYSTEM_INCLUDE_FRAME(\"${HEADER}\")\n")
endforeach()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SystemIncludeFrame/LeonSystemIncludeFrame.h" "${LEON_SYSTEM_INCLUDE_FRAME}")

target_include_directories(Leon.CLI PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/SystemIncludeFrame")

# Link libclang
set(LLVM_ROOT "")

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/libclang")
	set(LLVM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/libclang")
elseif (EXISTS "C:/Program Files/LLVM") # Default Windows LLVM directory
	set(LLVM_ROOT "C:/Program Files/LLVM")
endif()

if (LLVM_ROOT)
	message(STATUS "Using libclang install at ${LLVM_ROOT}")
	
	target_link_directories(Leon.CLI PRIVATE "${LLVM_ROOT}/bin")
	target_link_directories(Leon.CLI PRIVATE "${LLVM_ROOT}/lib")
	target_link_libraries(Leon.CLI PRIVATE libclang)

	target_include_directories(Leon.CLI PRIVATE "${LLVM_ROOT}/include")
else()
	find_package(Clang)

	if (TARGET libclang)
		message(STATUS "Using libclang from system install")
		message(STATUS "CLANG_INSTALL_PREFIX = ${CLANG_INSTALL_PREFIX}")

		target_link_libraries(Leon.CLI PRIVATE libclang)

		target_include_directories(Leon.CLI PRIVATE "${CLANG_INSTALL_PREFIX}/include")
	else()
		message(FATAL_ERROR "Could not find a libclang install.")
	endif()
endif()

# Compile and link luau
set(LUAU_BUILD_TESTS OFF)
add_subdirectory("ThirdParty/luau" EXCLUDE_FROM_ALL)

target_link_libraries(Leon.CLI PRIVATE Luau.Compiler Luau.VM)

# Project functions
function (leon_target LEON_TARGET LEON_BINARY_DIR CXX_TARGET LUA_PROCESS OUT_EXTENSION GLUE_EXTENSION)
	# Process arguments
	set(ARG_GLUE "${LEON_BINARY_DIR}/glue${GLUE_EXTENSION}")

	# If we pass relative source paths, make them relative to the CXX_TARGET
	set(ARG_SOURCES "")
	set(ARG_OUTPUTS "")

	get_target_property(CXX_SOURCE_DIR ${CXX_TARGET} SOURCE_DIR)
	foreach (arg ${ARGN})
		if (NOT IS_ABSOLUTE ${arg})
			set(arg "${CXX_SOURCE_DIR}/${arg}")
		endif()
		list(APPEND ARG_SOURCES "${arg}")

		set(arg_out "${arg}")

		string(REPLACE ":" "_" arg_out "${arg_out}")
		string(REPLACE "/" "_" arg_out "${arg_out}")
		string(REPLACE "\\" "_" arg_out "${arg_out}")

		list(APPEND ARG_OUTPUTS "${LEON_BINARY_DIR}/${arg_out}/out${OUT_EXTENSION}")
	endforeach()

	# Call Leon
	add_custom_command(
		OUTPUT ${ARG_GLUE} ${ARG_OUTPUTS}
		VERBATIM
		COMMAND Leon.CLI "${LEON_BINARY_DIR}" "${LUA_PROCESS}" -out_extension ${OUT_EXTENSION} -glue_extension ${GLUE_EXTENSION} -include "$<TARGET_PROPERTY:${CXX_TARGET},INCLUDE_DIRECTORIES>" -define "$<TARGET_PROPERTY:${CXX_TARGET},COMPILE_DEFINITIONS>" "${ARG_SOURCES}"
		DEPENDS Leon.CLI ${LUA_PROCESS} ${ARG_SOURCES}
	)

	add_custom_target(${LEON_TARGET} DEPENDS ${ARG_GLUE} ${ARG_OUTPUTS})

	# Exports
	set(${LEON_TARGET}_GLUE "${ARG_GLUE}" PARENT_SCOPE)
	set(${LEON_TARGET}_OUTPUTS "${ARG_OUTPUTS}" PARENT_SCOPE)
endfunction()

function (leon_target_outputs LEON_TARGET CXX_TARGET)
	target_sources(${CXX_TARGET} PUBLIC ${${LEON_TARGET}_OUTPUTS})
endfunction()

function (leon_target_glue LEON_TARGET CXX_TARGET)
	target_sources(${CXX_TARGET} PUBLIC ${${LEON_TARGET}_GLUE})
endfunction()

# Compile tests
if (LEON_BUILD_TESTS)
	add_subdirectory("Tests/General")
endif()
