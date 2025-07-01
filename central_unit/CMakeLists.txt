cmake_minimum_required(VERSION 3.21)

# Policy for boost compatibility
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(SmartHome)

# Default value for TARGET_DEVICE
set(TARGET_DEVICE "NATIVE" CACHE STRING "Target device type")
# Possible values for TARGET_DEVICE
set_property(CACHE TARGET_DEVICE PROPERTY STRINGS "NATIVE" "RPI_ZERO_2W")

# Config info
message(STATUS "==================[ Build Configuration ]=====================")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "Target Device: ${TARGET_DEVICE}")
message(STATUS "==============================================================")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Boost config
set(Boost_NO_WARN_NEW_VERSIONS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# Static boost linking for release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(Boost_USE_STATIC_LIBS ON)
else()
    set(Boost_USE_STATIC_LIBS OFF)
endif()

# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        system
        thread
        filesystem
        program_options
        chrono
)

add_executable(SmartHome
        src/main.cpp
        src/smart_home_core.cpp
        src/smart_home_tcp_server.cpp
)
add_executable(ModuleMediator
        src/module_mediator.cpp
)


target_link_libraries(SmartHome PRIVATE
        Boost::system
        Boost::thread
        Boost::filesystem
        Boost::program_options
        Boost::chrono
        pthread
)

target_link_libraries(ModuleMediator PRIVATE
        Boost::system
        Boost::program_options
        pthread
)


# Platform based config
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    target_compile_options(SmartHome PRIVATE
            -march=armv8-a
    )
endif()

if(TARGET_DEVICE STREQUAL "RPI_ZERO_2W")
    target_compile_options(SmartHome PRIVATE
            -mtune=cortex-a53
    )
endif ()


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Additional options for debuging
    target_compile_options(SmartHome PRIVATE
            -O0
            -g3
            -DDEBUG
    )
else ()
    # Additional options for release optimization
    target_compile_options(SmartHome PRIVATE
            -O2
            -DNDEBUG
            -ffunction-sections
            -fdata-sections
    )
    target_link_options(SmartHome PRIVATE
            -Wl,--gc-sections
    )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")
endif ()
