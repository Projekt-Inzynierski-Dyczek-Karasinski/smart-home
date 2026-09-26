set(test_names
        api_golden_test
        api_test
)

smarthome_add_test_group(api TESTS ${test_names} LIBS smarthome::api)