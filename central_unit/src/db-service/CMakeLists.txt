# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
        system
)


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
)


