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

add_library(smarthome::core_lib ALIAS smarthome_core_lib)

target_include_directories(smarthome_core_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${libical_SOURCE_DIR}/src/libical
        ${libical_BINARY_DIR}/src/libical
)

# TODO !pr check if needed
target_compile_features(smarthome_core_lib PUBLIC cxx_std_23)
target_compile_definitions(smarthome_core_lib PUBLIC BOOST_ASIO_HAS_CO_AWAIT)

target_link_libraries(smarthome_core_lib PUBLIC
        smarthome::api
        smarthome::ipc
        smarthome::utility
        smarthome::constants
        Boost::program_options
        nlohmann_json::nlohmann_json
        ical
        ical_cxx
)

if(ENABLE_COVERAGE)
    target_compile_options(smarthome_core_lib PRIVATE --coverage)
    target_link_options(smarthome_core_lib PRIVATE --coverage)
endif()

if (ENABLE_SYSTEMD)
    # Add compile flag to signal systemd use
    target_compile_definitions(smarthome_core_lib PUBLIC WITH_SYSTEMD)
    # Link systemd
    target_link_libraries(smarthome_core_lib PUBLIC ${SYSTEMD_LIBRARIES})
    target_include_directories(smarthome_core_lib PUBLIC ${SYSTEMD_INCLUDE_DIRS})
endif ()

# Target
add_executable(smarthomed main.cpp)
target_link_libraries(smarthomed PRIVATE smarthome::core_lib)

