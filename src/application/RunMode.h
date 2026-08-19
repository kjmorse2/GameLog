#pragma once

#include <optional>

namespace gamelog::application
{
    /**
     * @brief The mode the GameLog executable was asked to run in.
     */
    enum class RunMode
    {
        Headless, Gui, Live
    };

    /**
     * @brief Determine the run mode based on the command-line arguments.
     *
     * Exactly one recognized run-mode argument is required. Duplicate run-mode
     * arguments and unrecognized arguments are rejected.
     * @param argc of main.
     * @param argv of main.
     * @return enum RunMode of the options above, or std::nullopt for invalid arguments.
     */
    [[nodiscard]] std::optional<RunMode> determineRunMode(int argc, char* argv[]);
} // namespace gamelog::application
