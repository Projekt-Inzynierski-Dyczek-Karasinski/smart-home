cmake_minimum_required(VERSION 3.28)
project(SmartHome)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Policy for boost compatibility
if (POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif ()

# Define defaults for build variables
set(TARGET_DEVICE "NATIVE" CACHE STRING "Target device type")
set_property(CACHE TARGET_DEVICE PROPERTY STRINGS "NATIVE" "RPI_ZERO_2W")

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "CMake build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug")

# Build options
option(ENABLE_SYSTEMD "Enables systemd integration for core" ON)
option(BUILD_CORE "Build SmartHome daemon" ON)

option(BUILD_MEDIATOR "Build module mediator" ON)

option(BUILD_DB_SERVICE "Build SmartHome database daemon" ON)

option(BUILD_WEB_SERVER "Build SmartHome web server" ON)

option(BUILD_CLI "Build SmartHome CLI" ON)

# TODO rework GUI
option(BUILD_GUI "Build SmartHome GUI (old/incomplete)" OFF)

# Sanitizers options
option(ENABLE_ASAN "Address sanitizer + undefined behavior sanitizer" OFF)
option(ENABLE_TSAN "Thread sanitizer" OFF)

# Tests options
option(ENABLE_COVERAGE "Code coverage" OFF)
option(BUILD_TESTS "Build tests" OFF)

# Boost config
set(Boost_NO_WARN_NEW_VERSIONS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# Dynamic boost linking for debug
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(Boost_USE_STATIC_LIBS ON)
else ()
    set(Boost_USE_STATIC_LIBS OFF)
endif ()

# Config info
message(STATUS "==================[ Build Configuration ]=====================")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "Target Device: ${TARGET_DEVICE}")
message(STATUS "==============================================================")

# Platform based compile config
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    add_compile_options(-march=armv8-a)
endif ()

if (TARGET_DEVICE STREQUAL "RPI_ZERO_2W")
    add_compile_options(-mtune=cortex-a53)
endif ()


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Additional options for debuging
    add_compile_options(
            -O0
            -g3
            -DDEBUG
    )
else ()
    # Additional options for release optimization
    add_compile_options(
            -O2
            -DNDEBUG
            -ffunction-sections
            -fdata-sections
    )
    add_link_options(-Wl,--gc-sections,-s)
endif ()

# Enable coverage
if (ENABLE_COVERAGE AND CMAKE_CROSSCOMPILING)
    message(WARNING "Coverage is not enabled when cross-compiling, forcing ENABLE_COVERAGE=OFF")
    set(ENABLE_COVERAGE OFF)
endif ()

if (ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif ()

# Enable sanitizers
if (ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be enabled simultaneously")
endif ()

if ((ENABLE_ASAN OR ENABLE_TSAN) AND CMAKE_CROSSCOMPILING)
    message(WARNING "Sanitizers are not supported with cross-compilation, forcing ENABLE_ASAN=OFF and ENABLE_TSAN=OFF")
    set(ENABLE_ASAN OFF)
    set(ENABLE_TSAN OFF)
endif ()

if (ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif ()

if (ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif ()

# Additional warnings
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
        -Wall
        -Wextra
        -Wshadow=local
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Warith-conversion
        -Wuseless-cast
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wmisleading-indentation
        -Wsuggest-override
        -Wnull-dereference
)

# Add subdirectories
add_subdirectory(src/common)

if (BUILD_CORE)
    add_subdirectory(src/core)
endif ()

if (BUILD_MEDIATOR)
    add_subdirectory(src/mediator)
endif ()

if (BUILD_DB_SERVICE)
    add_subdirectory(src/db-service)
endif ()

if (BUILD_WEB_SERVER)
    add_subdirectory(src/web_server)
endif ()

if (BUILD_CLI)
    add_subdirectory(src/cli)
endif ()

if (BUILD_GUI)
    add_subdirectory(src/gui)
endif ()

# Add tests
if (BUILD_TESTS AND CMAKE_CROSSCOMPILING)
    message(WARNING "Tests are not built when cross-compiling, forcing BUILD_TESTS=OFF")
    set(BUILD_TESTS OFF)
endif ()

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

message(STATUS "======================[ Feature summary ]=======================")
include(FeatureSummary)
feature_summary(WHAT ALL)
message(STATUS "================================================================")