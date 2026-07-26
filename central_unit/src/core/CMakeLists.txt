# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
)

find_package(nlohmann_json 3.11.2 REQUIRED)

if (ENABLE_SYSTEMD)
    # Required PkgConfig for systemd
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
endif ()

include(FetchContent)

FetchContent_Declare(
        libical
        GIT_REPOSITORY https://github.com/libical/libical.git
        GIT_TAG v3.0.20
        GIT_SHALLOW ON
        SYSTEM
        EXCLUDE_FROM_ALL
)

set(STATIC_ONLY ON CACHE BOOL "" FORCE)
set(ICAL_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(LIBICAL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ICAL_GLIB OFF CACHE BOOL "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_ICU ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libical)
set_target_properties(ical ical-header ical_cxx icalss icalss-header icalss_cxx icalvcal
        PROPERTIES FOLDER "${THIRD_PARTY_TARGETS_FOLDER}")


# Core library
add_library(smarthome_core_lib STATIC
        core.cpp
        cache.cpp
        scheduler.cpp
        event_handler.cpp
        api/internal_api.cpp
        actions/actions.cpp
        actions/action_helpers.cpp
        actions/core_actions.cpp
        actions/mediator_actions.cpp
        actions/database_actions.cpp
)
set_target_properties(smarthome_core_lib PROPERTIES FOLDER "${LIBRARIES_TARGETS_FOLDER}")

add_library(smarthome::core_lib ALIAS smarthome_core_lib)

target_include_directories(smarthome_core_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(smarthome_core_lib SYSTEM PUBLIC
        ${libical_SOURCE_DIR}/src/libical
        ${libical_BINARY_DIR}/src/libical
)

target_link_libraries(smarthome_core_lib PUBLIC
        smarthome::api
        smarthome::ipc
        smarthome::utility
        smarthome::constants
        nlohmann_json::nlohmann_json
        ical
        ical_cxx
        PRIVATE
        project_warnings
)

if (ENABLE_SYSTEMD)
    # Add compile flag to signal systemd use
    target_compile_definitions(smarthome_core_lib PRIVATE WITH_SYSTEMD)
    # Link systemd
    target_link_libraries(smarthome_core_lib PRIVATE PkgConfig::SYSTEMD)
endif ()

# Target
add_executable(smarthomed main.cpp)
target_link_libraries(smarthomed PRIVATE
        smarthome::core_lib
        Boost::program_options
        project_warnings)

