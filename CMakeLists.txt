cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

project(whatishedoing)

option(BUILD_TESTING "Build dependency-free policy tests" OFF)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(NOT DEFINED ENV{GEODE_SDK})
    message(
        FATAL_ERROR
        "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode"
    )
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

set(ARC_FEATURE_FULL ON CACHE BOOL "" FORCE)

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})

if(BUILD_TESTING)
    enable_testing()
    add_executable(
        policy_tests
        tests/policy_tests.cpp
        src/level_filter.cpp
        src/play_policy.cpp
        src/retry_policy.cpp
        src/text_policy.cpp
    )
    target_include_directories(policy_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(policy_tests PRIVATE fmt::fmt-header-only)
    add_test(NAME policy_tests COMMAND policy_tests)
endif()
