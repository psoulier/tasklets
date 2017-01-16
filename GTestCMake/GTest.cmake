
include(CMakeParseArguments)

function(googletest_setup)

	set(oneValueArgs URL)

	cmake_parse_arguments(GTEST_SETUP "" "${oneValueArgs}" "" "${ARGN}")

	message("GTEST_SEUP_URL=${GTEST_SETUP_URL}")

	set(GTEST_SOURCE_DIR "${CMAKE_BINARY_DIR}/googletest-src")
	set(GTEST_BINARY_DIR "${CMAKE_BINARY_DIR}/googletest-build")

	set(GTEST_DOWNLD_DIR "${CMAKE_BINARY_DIR}/googletest-download")

	configure_file(
		"${CMAKE_CURRENT_LIST_DIR}/GTestCMake/GTestSetup.CMakeLists.cmake.in"
		"${GTEST_DOWNLD_DIR}/CMakeLists.txt"
	)


	execute_process(
		COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
		WORKING_DIRECTORY "${GTEST_DOWNLD_DIR}"
	)

	execute_process(COMMAND ${CMAKE_COMMAND} --build .
		WORKING_DIRECTORY "${GTEST_DOWNLD_DIR}"
	)

	add_subdirectory(${GTEST_SOURCE_DIR} ${GTEST_BINARY_DIR})

endfunction()

function(googletest_add target)
	add_executable(${target} ${ARGN})
	target_link_libraries(${target} gtest gtest_main)
	add_test(${target} ${target})
endfunction()
