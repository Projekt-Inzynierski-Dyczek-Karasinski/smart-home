# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
        system
)

find_package(PostgreSQL 15.15 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx>=6.4)

# Target
add_executable(smarthome-database
        main.cpp
)


target_link_libraries(smarthome-database PRIVATE
        smarthome::ipc
        smarthome::api
        smarthome::utility
        smarthome::logger
        Boost::program_options
        Boost::system
        ${PostgreSQL_LIBRARIES}
        ${PQXX_LIBRARIES}
)

target_include_directories(smarthome-database PRIVATE ${PQXX_INCLUDE_DIRS})



