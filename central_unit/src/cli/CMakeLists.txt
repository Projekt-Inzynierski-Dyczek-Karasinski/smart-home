# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
)

add_executable(smarthomectl
        main.cpp
)

target_link_libraries(smarthomectl PRIVATE
        smarthome::ipc
        smarthome::logger
        smarthome::warnings
        Boost::program_options
)