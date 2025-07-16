# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
)


# Target
add_executable(smarthomed
        main.cpp
        core.cpp
)


target_link_libraries(smarthomed PRIVATE
        smarthome::ipc
        smarthome::utility
        Boost::program_options
)

