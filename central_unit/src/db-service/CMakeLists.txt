# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
        system
)


include(FetchContent)

FetchContent_Declare(
        libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG 7.10.5
        GIT_SHALLOW ON
)

# Build libpqxx static
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(SKIP_BUILD_TEST ON CACHE INTERNAL "")
set(SKIP_BUILD_TEST ON CACHE INTERNAL "")
set(BUILD_DOC OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(libpqxx)

find_package(PostgreSQL 15.15 REQUIRED)

# Target
add_executable(smarthome-database
        main.cpp
        database_service.cpp
        database_connection.cpp
        database_connection_manager.cpp
        database_api.cpp
        database_client.cpp
)


target_link_libraries(smarthome-database PRIVATE
        smarthome::ipc
        smarthome::api
        smarthome::utility
        smarthome::logger
        smarthome::constants
        Boost::program_options
        Boost::system
        pqxx
        ${PostgreSQL_LIBRARIES}
)

target_include_directories(smarthome-database PRIVATE ${PQXX_INCLUDE_DIRS})



