# Boost required
find_package(Boost 1.74 REQUIRED COMPONENTS
        program_options
        system
)

# Require PkgConfig for libgpiod
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiodcxx)

# Target
add_executable(smarthome-mediator
        main.cpp
        mediator.cpp
        api_client.cpp
        rf_client.cpp
        rf_driver/hc12_driver.cpp
        rf_driver/uart_port.cpp
        rf_driver/gpio_handler.cpp
        rf_api.cpp
        rf_api.h
)


target_link_libraries(smarthome-mediator PRIVATE
        smarthome::ipc
        smarthome::api
        smarthome::utility
        smarthome::logger
        Boost::program_options
        Boost::system
        PkgConfig::GPIOD
)


