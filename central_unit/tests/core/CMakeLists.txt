set(test_names
        event_handler_test
        config_cache_test
        readings_cache_test
)

smarthome_add_test_group(core TESTS ${test_names} LIBS smarthome::core_lib)