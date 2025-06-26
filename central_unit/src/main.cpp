#include "smart_home_core.h"

#include <iostream>
#include <map>
#include <string>

enum Options {
    optionInvalid = 0,
    help = 1,
};

Options resolveOption(const std::string &input) {
    static const std::map<std::string, Options> optionsMap{
        {"--help", help},
        {"-h", help}
    };

    auto itr = optionsMap.find(input);
    if (itr != optionsMap.end()) {
        return itr->second;
    }
    return optionInvalid;
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        switch (resolveOption(arg)) {
            case 1:
                std::cout << "HELP" << std::endl;
                //TODO add print help
                return 0;
            default:
                std::cerr << "Unknown option: " << arg << std::endl;
                return 1;
        }
    }

    try {
        auto &core = SmartHome::Core::Instance();

        if (core.initialize() == false) {
            std::cerr << "Failed to initialize core" << std::endl;
            return 1;
        }

        core.run();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
