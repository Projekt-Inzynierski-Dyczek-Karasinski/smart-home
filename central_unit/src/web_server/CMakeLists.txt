# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
        system
)

include(FetchContent)

FetchContent_Declare(
        Crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG v1.3.0
        GIT_SHALLOW ON
        SYSTEM
        EXCLUDE_FROM_ALL
)

set(CROW_USE_BOOST ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Crow)

# Target
add_executable(smarthome-web-server
        main.cpp
        api_client.cpp
        web_server.cpp
        routes/routes.cpp
        routes/route_helpers.cpp
        routes/core_routes.cpp
        routes/module_routes.cpp
        routes/devices_routes.cpp
        routes/database_routes.cpp
        routes/static_routes.cpp
)


target_link_libraries(smarthome-web-server PRIVATE
        smarthome::ipc
        smarthome::api
        smarthome::utility
        smarthome::logger
        smarthome::constants
        Boost::program_options
        Boost::system
        Crow::Crow
        project_warnings
)


