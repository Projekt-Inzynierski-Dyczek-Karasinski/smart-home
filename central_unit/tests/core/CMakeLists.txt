add_executable(event_handler_test event_handler_test.cpp)

target_link_libraries(event_handler_test PRIVATE
        smarthome::core_lib
        GTest::gtest_main)

if(ENABLE_COVERAGE)
    target_compile_options(event_handler_test PRIVATE --coverage)
    target_link_options(event_handler_test PRIVATE --coverage)
endif()

gtest_discover_tests(event_handler_test)