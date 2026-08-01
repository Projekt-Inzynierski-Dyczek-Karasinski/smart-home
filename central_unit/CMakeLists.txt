cmake_minimum_required(VERSION 3.28...4.3)
project(SmartHome)

if (MSVC)
    message(FATAL_ERROR "MSVC not supported")
endif ()

set(CMAKE_CXX_STANDARD 23) # TODO delete after refactoring targets definitions using helpers
set(CMAKE_CXX_STANDARD_REQUIRED ON) # TODO delete after refactoring targets definitions using helpers
set(CMAKE_CXX_EXTENSIONS OFF)

# TODO delete after dependencies update
# Policy for boost compatibility
if (POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif ()

# Define folders for target organization
set(LIBRARIES_TARGETS_FOLDER "Libs" CACHE STRING "Folder where libraries targets are placed")
set(THIRD_PARTY_TARGETS_FOLDER "ThirdParty" CACHE STRING
        "Folder where third party libraries and tools targets are placed")
set(TESTS_TARGETS_FOLDER "Tests" CACHE STRING "Folder where tests targets are placed")

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
message(STATUS "CMAKE_VERSION: ${CMAKE_VERSION}")
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

# Set release flags
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Additional options for debuging
    add_compile_options(-g3)
    add_compile_definitions(DEBUG)
else ()
    # Additional options for release optimization
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections -s)
endif ()

# Enable coverage
set(coverage_enabled ${ENABLE_COVERAGE})
if (coverage_enabled AND CMAKE_CROSSCOMPILING)
    message(WARNING "Coverage is not enabled when cross-compiling")
    set(coverage_enabled OFF)
endif ()

# TODO Change to library interface (similar to warnings) and add directly to targets in smarthome_targets helpers
#      Consider same thing for sanitizers
if (coverage_enabled)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif ()


# Enable sanitizers
set(asan_enabled ${ENABLE_ASAN})
set(tsan_enabled ${ENABLE_TSAN})
if (asan_enabled AND tsan_enabled)
    message(FATAL_ERROR "ASan and TSan cannot be enabled simultaneously")
endif ()

if ((asan_enabled OR tsan_enabled) AND CMAKE_CROSSCOMPILING)
    message(WARNING "Sanitizers are not supported with cross-compilation")
    set(asan_enabled OFF)
    set(tsan_enabled OFF)
endif ()

if (asan_enabled)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif ()

if (tsan_enabled)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif ()

# Additional warnings
add_library(smarthome_warnings INTERFACE)
add_library(smarthome::warnings ALIAS smarthome_warnings)

target_compile_options(smarthome_warnings INTERFACE
        -Wall
        -Wextra
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
        -Wmisleading-indentation
        -Wsuggest-override
        -Wnull-dereference
)

# Additional warnings GCC only
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(smarthome_warnings INTERFACE
            -Wshadow=local
            -Warith-conversion
            -Wuseless-cast
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
    )
endif ()

set(SMARTHOME_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add cmake files
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(smarthome_targets) # Target helpers

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
set(build_test_enabled ${BUILD_TESTS})
if (build_test_enabled AND CMAKE_CROSSCOMPILING)
    message(WARNING "Tests are not built when cross-compiling")
    set(build_test_enabled OFF)
endif ()

if (build_test_enabled)
    enable_testing()
    add_subdirectory(tests)
endif ()

message(STATUS "======================[ Feature summary ]=======================")
include(FeatureSummary)
feature_summary(WHAT ALL)
message(STATUS "================================================================")