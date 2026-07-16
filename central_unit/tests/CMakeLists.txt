include(FetchContent)
FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW ON
        SYSTEM
        EXCLUDE_FROM_ALL)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)
set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES FOLDER "third_party")

include(GoogleTest)

# Add list of tests to global all_tests target and group target
function(smarthome_add_test_group GROUP)
    cmake_parse_arguments(
            ARG
            ""
            ""
            "TESTS;LIBS"
            ${ARGN}
    )

    set(sources "")
    foreach (name IN LISTS ARG_TESTS)
        list(APPEND sources "${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp")
    endforeach ()

    add_executable(${GROUP}_tests ${sources})
    set_target_properties(${GROUP}_tests PROPERTIES FOLDER "tests")
    target_link_libraries(${GROUP}_tests
            PUBLIC
            GTest::gtest
            GTest::gmock
            ${ARG_LIBS}
            PRIVATE
            GTest::gtest_main
            project_warnings
    )

    gtest_discover_tests(${GROUP}_tests
            TEST_PREFIX "${GROUP}."
            PROPERTIES LABELS "${GROUP}"
    )

    set_property(GLOBAL APPEND PROPERTY SMARTHOME_TEST_SOURCES ${sources})
    set_property(GLOBAL APPEND PROPERTY SMARTHOME_TEST_LIBS ${ARG_LIBS})
endfunction()

# Tests subdirectories
add_subdirectory(core)


# All tests in one target
get_property(all_sources GLOBAL PROPERTY SMARTHOME_TEST_SOURCES)
get_property(all_libs GLOBAL PROPERTY SMARTHOME_TEST_LIBS)

add_executable(all_tests ${all_sources})
set_target_properties(all_tests PROPERTIES FOLDER "tests")
target_link_libraries(all_tests
        PUBLIC
        GTest::gtest
        GTest::gmock
        ${all_libs}
        PRIVATE
        GTest::gtest_main
        project_warnings
)