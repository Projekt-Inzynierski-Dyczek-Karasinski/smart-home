# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
)

if (ENABLE_SYSTEMD)
    # Required PkgConfig for systemd
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SYSTEMD REQUIRED libsystemd)

endif ()


# Target
add_executable(smarthomed
        main.cpp
        core.cpp
        service/service_manager.cpp
        service/systemd_service.cpp
        service/standalone_service.cpp
)


if (ENABLE_SYSTEMD)
    # Add compile flag to signal systemd use
    target_compile_definitions(smarthomed PRIVATE WITH_SYSTEMD)
    # Link systemd
    target_link_libraries(smarthomed PRIVATE ${SYSTEMD_LIBRARIES})
    target_include_directories(smarthomed PRIVATE ${SYSTEMD_INCLUDE_DIRS})
endif ()


target_link_libraries(smarthomed PRIVATE
        smarthome::ipc
        smarthome::utility
        Boost::program_options
)

