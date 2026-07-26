# Fetching yaml-cpp from github for static build
include(FetchContent)

find_package(ZLIB REQUIRED)

FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.9.0
        GIT_SHALLOW ON
        SYSTEM
        EXCLUDE_FROM_ALL
)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(yaml-cpp)

# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        system
        regex
        iostreams
)

if (ENABLE_SYSTEMD)
    # Required PkgConfig for systemd
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
endif ()


# SMARTHOME LOGGER
add_library(smarthome_logger STATIC
        logger/logger.cpp
        logger/async_logger.cpp
)
set_target_properties(smarthome_logger PROPERTIES FOLDER "lib")


add_library(smarthome::logger ALIAS smarthome_logger)

target_include_directories(smarthome_logger PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/logger>
)

target_link_libraries(smarthome_logger PRIVATE
        Boost::system
        project_warnings
)

# SMARTHOME API
add_library(smarthome_api STATIC
        api/api.cpp
)
set_target_properties(smarthome_api PROPERTIES FOLDER "lib")

add_library(smarthome::api ALIAS smarthome_api)

target_include_directories(smarthome_api PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
)

target_link_libraries(smarthome_api PRIVATE
        project_warnings
)

# SMARTHOME IPC
add_library(smarthome_ipc STATIC
        ipc/socket_client.cpp
        ipc/socket_connection.cpp
        ipc/socket_server_connection.cpp
        ipc/socket_server.cpp
)
set_target_properties(smarthome_ipc PROPERTIES FOLDER "lib")

add_library(smarthome::ipc ALIAS smarthome_ipc)

target_include_directories(smarthome_ipc PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ipc>
)

target_link_libraries(smarthome_ipc PRIVATE
        Boost::system
        Boost::regex
        Boost::iostreams
        smarthome::logger
        smarthome::api
        ZLIB::ZLIB
        project_warnings
)

# SMARTHOME UTILITY
add_library(smarthome_utility STATIC
        utils/utils.cpp
        utils/config_manager/config_manager.cpp
        utils/service_manager/service_manager.cpp
        utils/service_manager/standalone_service.cpp
        utils/service_manager/systemd_service.cpp
)
set_target_properties(smarthome_utility PROPERTIES FOLDER "lib")


add_library(smarthome::utility ALIAS smarthome_utility)


target_include_directories(smarthome_utility PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils>

)

if (ENABLE_SYSTEMD)
    # Add compile flag to signal systemd use
    target_compile_definitions(smarthome_utility PRIVATE WITH_SYSTEMD)
    # Link systemd
    target_link_libraries(smarthome_utility PUBLIC PkgConfig::SYSTEMD)
endif ()

target_link_libraries(smarthome_utility
        PRIVATE
        yaml-cpp
        project_warnings
        PUBLIC
        smarthome::logger
)

# SMARTHOME CONSTANTS
add_library(smarthome_constants INTERFACE)
add_library(smarthome::constants ALIAS smarthome_constants)

target_include_directories(smarthome_constants INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/constants>
)