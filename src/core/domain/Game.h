#pragma once

#include <optional>

#include <QString>

namespace gamelog::core::domain
{
    /**
     * @brief Describes one registered game entry.
     *
     * The struct intentionally stays lightweight so repositories and session
     * services can pass it around by value.
     */
    struct Game
    {
        /**
         * @brief Primary key from the games table.
         */
        int id{0};

        /**
         * @brief Display title shown in the UI.
         */
        QString title;

        /**
         * @brief Absolute path to the executable used for process matching.
         */
        QString executablePath;

        /**
         * @brief Basename of the executable, cached for display and matching.
         */
        QString executableName;

        /**
         * @brief Optional Steam application identifier.
         */
        std::optional<int> steamAppId;

        /**
         * @brief Optional local artwork file path.
         */
        bool hasArtwork{false};

        /**
         * @brief True when the game should be considered by detection logic.
         */
        bool trackingEnabled{true};
    };

    QDebug operator<<(QDebug debug, const Game& game);
} // namespace gamelog::core::domain
