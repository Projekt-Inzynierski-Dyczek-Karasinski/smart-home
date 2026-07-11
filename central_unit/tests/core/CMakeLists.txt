add_executable(event_handler_test event_handler_test.cpp)

target_link_libraries(event_handler_test PRIVATE
        smarthome::core_lib
        GTest::gtest_main
        project_warnings)

gtest_discover_tests(event_handler_test TEST_PREFIX "core.")