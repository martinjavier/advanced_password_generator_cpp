cmake_minimum_required(VERSION 3.16)

project(apg
    VERSION 0.1.0
    DESCRIPTION "Advanced Password Generator - command-line password generator"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Core logic as a library, shared by the executable and the tests.
add_library(apg_core STATIC src/apg.cpp)
target_include_directories(apg_core PUBLIC include)

if(MSVC)
    target_compile_options(apg_core PRIVATE /W4)
    target_link_libraries(apg_core PUBLIC bcrypt)
else()
    target_compile_options(apg_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(apg src/main.cpp)
target_link_libraries(apg PRIVATE apg_core)

install(TARGETS apg RUNTIME DESTINATION bin)

# Tests
include(CTest)
if(BUILD_TESTING)
    add_executable(apg_tests tests/test_apg.cpp)
    target_link_libraries(apg_tests PRIVATE apg_core)
    add_test(NAME apg_tests COMMAND apg_tests)
endif()
