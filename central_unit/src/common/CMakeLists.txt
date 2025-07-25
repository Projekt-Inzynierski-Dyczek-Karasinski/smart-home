# Yaml-cpp required
find_package(yaml-cpp REQUIRED)
# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        system
        regex
)


add_library(smarthome_ipc STATIC
        ipc/socket_connection.cpp
        ipc/socket_server_connection.cpp
        ipc/socket_server.cpp

)

add_library(smarthome::ipc ALIAS smarthome_ipc)

target_include_directories(smarthome_ipc PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ipc>

)

target_link_libraries(smarthome_ipc PUBLIC
        Boost::system
        Boost::regex
)


add_library(smarthome_utility STATIC
        utils/config_manager.cpp
        utils/utils.cpp
)

add_library(smarthome::utility ALIAS smarthome_utility)


target_include_directories(smarthome_utility PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils>

)


target_link_libraries(smarthome_utility PRIVATE
        yaml-cpp

)