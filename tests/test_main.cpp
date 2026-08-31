#include "test_framework.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

arkweb::material::test::RunOptions ParseOptions(int argc, char** argv)
{
    arkweb::material::test::RunOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--list") {
            options.list_only = true;
        } else if (argument.rfind("--filter=", 0U) == 0U) {
            options.filter = argument.substr(9U);
        } else if (argument.rfind("--repeat=", 0U) == 0U) {
            options.repeat = std::max(1, std::atoi(argument.substr(9U).c_str()));
        } else if (argument == "--help") {
            std::cout << "Usage: web_material_tests [--list] [--filter=text] [--repeat=N]\n";
            std::exit(0);
        } else {
            std::cerr << "Unknown test option: " << argument << '\n';
            std::exit(2);
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv)
{
    return arkweb::material::test::RunAll(ParseOptions(argc, argv));
}
