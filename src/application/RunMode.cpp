#include "application/RunMode.h"

#include <cstring>

namespace gamelog::application
{
    std::optional<RunMode> determineRunMode(int argc, char* argv[])
    {
        if(argc != 2 || argv == nullptr || argv[1] == nullptr) { return std::nullopt; }

        if(std::strcmp(argv[1], "--headless") == 0) { return RunMode::Headless; }
        if(std::strcmp(argv[1], "--gui") == 0) { return RunMode::Gui; }
        if(std::strcmp(argv[1], "--live") == 0) { return RunMode::Live; }

        return std::nullopt;
    }
} // namespace gamelog::application
