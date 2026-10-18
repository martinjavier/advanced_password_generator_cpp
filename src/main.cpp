#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "apg.hpp"

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    const apg::ParseResult result = apg::parse_args(args);

    switch (result.kind) {
        case apg::ParseResult::Kind::Help:
            std::cout << apg::help_text() << '\n';
            return 0;

        case apg::ParseResult::Kind::Version:
            std::cout << "apg " << apg::VERSION << '\n';
            return 0;

        case apg::ParseResult::Kind::Generate:
            try {
                const auto& cfg = result.config;
                for (std::size_t i = 0; i < cfg.count; ++i) {
                    std::cout << apg::generate(cfg.length, cfg.groups) << '\n';
                }
                std::cout.flush();
                return std::cout ? 0 : 1;
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << '\n';
                return 1;
            }

        case apg::ParseResult::Kind::Error:
            break;
    }

    std::cerr << "Error: " << result.error << '\n'
              << "Run 'apg --help' for usage.\n";
    return 2;
}
