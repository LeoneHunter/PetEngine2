#define DOCTEST_CONFIG_IMPLEMENT
#include "base/command_line.h"
#include "thirdparty/doctest/doctest.h"

int main(int argc, char** argv) {
    CommandLine::Set(argc, argv);
    logging::SetLevel(logging::Level::Error);

    if(auto res = CommandLine::ParseArg<std::string>("-log_level")) {
        const auto str = ToLower(*res);
        if (str == "verbose") {
            logging::SetLevel(logging::Level::Verbose);
        } else if (str == "info") {
            logging::SetLevel(logging::Level::Info);
        } else if (str == "warning") {
            logging::SetLevel(logging::Level::Warning);
        } else if (str == "error") {
            logging::SetLevel(logging::Level::Error);
        }
    }
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run(); 
}