# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
)


# Target
add_executable(smarthome-mediator
        main.cpp

)


target_link_libraries(smarthome-mediator PRIVATE
        smarthome::ipc
        Boost::program_options
)


