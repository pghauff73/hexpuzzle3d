#include "application.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

hexpuzzle::HexPuzzleApplication::Options parseOptions(int argc, char** argv) {
    hexpuzzle::HexPuzzleApplication::Options options;
    options.randomSeed = static_cast<std::uint32_t>(
        std::chrono::system_clock::now().time_since_epoch().count());

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto requireValue = [&]() -> const char* {
            if (index + 1 >= argc) {
                throw std::invalid_argument("missing value after " + argument);
            }
            return argv[++index];
        };
        if (argument == "--seed") {
            options.randomSeed = static_cast<std::uint32_t>(std::stoul(requireValue()));
        } else if (argument == "--assets") {
            options.assetDirectory = std::filesystem::path(requireValue());
        } else if (argument == "--subdivisions") {
            options.subdivisionLevel = std::stoi(requireValue());
        } else if (argument == "--debug-log") {
            options.debugLogPath = std::filesystem::path(requireValue());
        } else if (argument == "--smoke-test") {
            options.smokeTest = true;
        } else if (argument == "--help") {
            std::cout
                << "Usage: hexp_main [--seed N] [--assets PATH] [--subdivisions 0..5] "
                   "[--debug-log PATH] [--smoke-test]\n"
                << "Move the pointer to orbit and select; left-click rotates the selected tile.\n";
            std::exit(EXIT_SUCCESS);
        } else {
            throw std::invalid_argument("unknown option: " + argument);
        }
    }
    if (options.subdivisionLevel < 0 || options.subdivisionLevel > 5) {
        throw std::invalid_argument("--subdivisions must be between 0 and 5");
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        hexpuzzle::HexPuzzleApplication application(parseOptions(argc, argv));
        return application.run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "hexpuzzle3d: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
