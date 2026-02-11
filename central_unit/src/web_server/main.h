#pragma once
#include "async_logger.h"
#include "constants.h"
#include "service_manager/service_manager.h"
#include "api_client.h"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

namespace SmartHomeWebServer {
    namespace ba = boost::asio;
    namespace bpo = boost::program_options;
    namespace bs = boost::system;
    namespace su = SmartHome::Utils;

    struct Config {
        ApiClient::Config apiClientConfig{};
    };
}

int main(int argc, char *argv[]);
