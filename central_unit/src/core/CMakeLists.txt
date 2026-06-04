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

FetchContent_Declare(
        libical
        GIT_REPOSITORY https://github.com/libical/libical.git
        GIT_TAG v3.0.20
)

set(STATIC_ONLY ON CACHE BOOL "" FORCE)
set(ICAL_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ICAL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ICAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ICAL_GLIB OFF CACHE BOOL "" FORCE)
set(ICAL_GTK_DOC OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libical)


# Target
add_executable(smarthomed
        main.cpp
        core.cpp
        cache.cpp
        scheduler.cpp
        api/internal_api.cpp
        actions/actions.cpp
        actions/action_helpers.cpp
        actions/core_actions.cpp
        actions/mediator_actions.cpp
        actions/database_actions.cpp
)

target_include_directories(smarthomed PRIVATE
        ${libical_SOURCE_DIR}/src/libical
        ${libical_BINARY_DIR}/src/libical
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
        smarthome::constants
        Boost::program_options
        nlohmann_json::nlohmann_json
        ical
        ical_cxx
)

