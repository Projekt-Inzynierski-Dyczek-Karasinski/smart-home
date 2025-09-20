# Yaml-cpp required
find_package(yaml-cpp REQUIRED)
# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        system
        regex
)


# SMARTHOME LOGGER
add_library(smarthome_logger STATIC
        utils/logger/logger.cpp
        utils/logger/async_logger.cpp
)

add_library(smarthome::logger ALIAS smarthome_logger)

target_include_directories(smarthome_logger PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils/logger>
)

target_link_libraries(smarthome_logger PRIVATE
        Boost::system
)

# SMARTHOME API
add_library(smarthome_api STATIC
        api/api.cpp
)

add_library(smarthome::api ALIAS smarthome_api)

target_include_directories(smarthome_api PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
)

# SMARTHOME IPC
add_library(smarthome_ipc STATIC
        ipc/socket_connection.cpp
        ipc/socket_server_connection.cpp
        ipc/socket_server.cpp
)

add_library(smarthome::ipc ALIAS smarthome_ipc)

target_include_directories(smarthome_ipc PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ipc>
)

target_link_libraries(smarthome_ipc PRIVATE
        Boost::system
        Boost::regex
        smarthome::logger
        smarthome::api
)

# SMARTHOME UTILITY
add_library(smarthome_utility STATIC
        utils/config_manager.cpp
        utils/utils.cpp
)

add_library(smarthome::utility ALIAS smarthome_utility)


target_include_directories(smarthome_utility PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils>

)

target_link_libraries(smarthome_utility
        PRIVATE
        yaml-cpp
        PUBLIC
        smarthome::logger
)