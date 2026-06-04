cmake_minimum_required(VERSION 3.21)
project(SmartHome)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Policy for boost compatibility
if (POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif ()


# Default value for TARGET_DEVICE
set(TARGET_DEVICE "NATIVE" CACHE STRING "Target device type")
# Possible values for TARGET_DEVICE
set_property(CACHE TARGET_DEVICE PROPERTY STRINGS "NATIVE" "RPI_ZERO_2W")

# Build options
option(ENABLE_SYSTEMD "Enables systemd integration for core" ON)
option(BUILD_CORE "Build SmartHome daemon" ON)

option(BUILD_MEDIATOR "Build module mediator" ON)

option(BUILD_DB_SERVICE "Build SmartHome database daemon" ON)

option(BUILD_WEB_SERVER "Build SmartHome web server" ON)

option(BUILD_CLI "Build SmartHome CLI" ON)

option(BUILD_GUI "Build SmartHome GUI" ON)

# Boost config
set(Boost_NO_WARN_NEW_VERSIONS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# Static boost linking for release
if (CMAKE_BUILD_TYPE STREQUAL "Release")
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
    add_link_options(-Wl,--gc-sections, -s)
endif ()

message(STATUS "======================[ Feature summary ]=======================")
include(FeatureSummary)
feature_summary(WHAT ALL)
message(STATUS "================================================================")