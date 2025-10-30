# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
)

find_package(nlohmann_json 3.11.2 REQUIRED)

if (ENABLE_SYSTEMD)
    # Required PkgConfig for systemd
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SYSTEMD REQUIRED libsystemd)

endif ()


# Target
add_executable(smarthomed
        main.cpp
        core.cpp
        core_actions.cpp
        api/internal_api.cpp
)


if (ENABLE_SYSTEMD)
    # Add compile flag to signal systemd use
    target_compile_definitions(smarthomed PRIVATE WITH_SYSTEMD)
    # Link systemd
    target_link_libraries(smarthomed PRIVATE ${SYSTEMD_LIBRARIES})
    target_include_directories(smarthomed PRIVATE ${SYSTEMD_INCLUDE_DIRS})
endif ()

target_compile_features(smarthomed PRIVATE cxx_std_23)
target_compile_definitions(smarthomed PRIVATE BOOST_ASIO_HAS_CO_AWAIT)

target_link_libraries(smarthomed PRIVATE
        smarthome::api
        smarthome::ipc
        smarthome::utility
        Boost::program_options
        nlohmann_json::nlohmann_json
)

