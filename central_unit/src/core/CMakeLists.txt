# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
)


# Target
add_executable(smarthomed
        main.cpp
        core.cpp
)


target_include_directories(smarthomed PRIVATE
        smarthome_ipc
        smarthome_utility
)


if (SYSTEMD_FOUND) #TODO set flag if found for if macro in cpp
    target_link_libraries(smarthomed PRIVATE ${SYSTEMD_LIBRARIES})
    target_include_directories(smarthomed PRIVATE ${SYSTEMD_INCLUDE_DIRS})
endif ()


target_link_libraries(smarthomed PRIVATE
        smarthome_utility
        smarthome_ipc
        Boost::program_options
)

