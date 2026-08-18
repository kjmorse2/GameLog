#pragma once

#include <optional>

#include <QDebug>
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
         * @brief True when a valid local cover.jpg is available for this game.
         *
         * The flag is persisted as a non-null boolean. Header and logo files do
         * not currently affect this value.
         */
        bool hasArtwork{false};

        /**
         * @brief True when the game should be considered by detection logic.
         */
        bool trackingEnabled{true};
    };

    /**
     * Allows for easy logging of Game objects using QDebug.
     * @param debug The QDebug stream to write to.
     * @param game The Game object to log.
     * @return The QDebug stream after writing the Game object.
     */
    QDebug operator<<(QDebug debug, const Game& game);
} // namespace gamelog::core::domain
