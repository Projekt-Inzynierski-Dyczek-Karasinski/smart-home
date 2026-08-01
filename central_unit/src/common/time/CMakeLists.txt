smarthome_add_library(time
        HEADERS time_provider.h
)

smarthome_add_library(time_asio
        HEADERS asio_time_provider.h
        PUBLIC_DEPS smarthome::time Boost::headers Boost::system
)

smarthome_add_library(time_testing
        HEADERS testing/fake_time_provider.h
        PUBLIC_DEPS smarthome::time
)