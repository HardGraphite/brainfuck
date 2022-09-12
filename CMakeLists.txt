cmake_minimum_required(VERSION 3.9)

file(READ "${CMAKE_SOURCE_DIR}/version.txt" HGBF_VERSION)

project(hgbf
	VERSION ${HGBF_VERSION}
	DESCRIPTION "HardGraphite's Brainfuck interpreter."
	LANGUAGES C
)

option(HGBF_TEST "Add tests." ON)
option(HGBF_PACK "Enable packing." ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
	message("`CMAKE_BUILD_TYPE` not set; use value \"${CMAKE_BUILD_TYPE}\"")
endif()

aux_source_directory(. hgbf_src)
add_executable(hgbf ${hgbf_src})
target_compile_definitions(hgbf PRIVATE "HGBF_VERSION=\"hgbf ${HGBF_VERSION}\"")

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(hgbf PRIVATE
		/W4 /utf-8 /Zc:inline,preprocessor
		/wd4200 /wd4706
	)
	target_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else() # GCC, Clang, ...
	target_compile_options(hgbf PRIVATE
		-pipe -Wall -Wextra -Wpedantic
		-Werror=incompatible-pointer-types -Werror=return-type
		-fno-plt -fno-exceptions
	)
endif()

if(HGBF_TEST)
	function(test_file file_name pass_regex)
		add_test(NAME ${file_name}
			COMMAND "$<TARGET_FILE:hgbf>" "${CMAKE_SOURCE_DIR}/test/${file_name}"
			WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
		)
		set_tests_properties(${file_name} PROPERTIES
			PASS_REGULAR_EXPRESSION "${pass_regex}"
		)
	endfunction()

	function(test_file_with_input file_name input_str pass_regex)
		set(input_file "${CMAKE_BINARY_DIR}/${file_name}.input")
		file(WRITE "${input_file}" "${input_str}")
		add_test(NAME ${file_name}
			COMMAND "$<TARGET_FILE:hgbf>" "-I" "${input_file}"
				"${CMAKE_SOURCE_DIR}/test/${file_name}"
			WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
		)
		set_tests_properties(${file_name} PROPERTIES
			PASS_REGULAR_EXPRESSION "${pass_regex}"
		)
	endfunction()

	enable_testing()
	test_file("adding.bf" "7")
	test_file("hello.bf" "Hello World!")
	test_file_with_input("rot13.bf" "Hello, brainfuck!" "Uryyb, oenvashpx!")
endif()

if (HGBF_PACK)
	if(UNIX)
		set(bin_dest "bin")
	else()
		set(bin_dest ".")
	endif()
	install(TARGETS hgbf DESTINATION ${bin_dest})

	set(CPACK_STRIP_FILES TRUE)
	set(CPACK_PACKAGE_NAME "hgbf")
	set(CPACK_PACKAGE_VENDOR "HardGraphite")
	set(CPACK_PACKAGE_CHECKSUM MD5)
	if(UNIX)
		set(CPACK_GENERATOR "TGZ")
	else()
		set(CPACK_GENERATOR "ZIP")
	endif()
	include(CPack)
endif()
