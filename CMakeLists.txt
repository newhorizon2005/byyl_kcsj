cmake_minimum_required(VERSION 3.16)

project(pl0_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

file(GLOB_RECURSE PL0_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(pl0c ${PL0_SOURCES})

target_compile_options(pl0c PRIVATE -Wall -Wextra -pedantic)

find_program(BASH_EXECUTABLE bash)

if(BASH_EXECUTABLE)
    enable_testing()

    add_test(
        NAME pl0_run_all
        COMMAND
            "${CMAKE_COMMAND}" -E env
            "PL0C=$<TARGET_FILE:pl0c>"
            "EXPECT_PARSE_LR=implemented"
            "${BASH_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_all.sh"
    )

    add_custom_target(
        check
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
        DEPENDS pl0c
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()
