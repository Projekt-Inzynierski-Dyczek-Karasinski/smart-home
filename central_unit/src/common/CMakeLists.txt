# Yaml-cpp required
find_package(yaml-cpp REQUIRED)
# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        system
)


add_library(smarthome_ipc STATIC
        ipc/tcp/tcp_server.cpp
        ipc/tcp/tcp_connection.cpp
)

target_include_directories(smarthome_ipc PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ipc>

)

target_link_libraries(smarthome_ipc PUBLIC
        Boost::system

)


add_library(smarthome_utility STATIC
        utils/config.cpp
)


target_include_directories(smarthome_utility PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils>

)


target_link_libraries(smarthome_ipc PRIVATE
        yaml-cpp

)