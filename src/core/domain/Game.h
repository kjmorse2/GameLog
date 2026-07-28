#pragma once

#include <optional>

#include <QString>

namespace gamelog::core::domain
{
/**
 * @brief The Game struct represents a game in the application.
 */
struct Game
{
    /**
     * @brief The unique identifier for the game.
     */
    int id{0};

    /**
     * @brief The title of the game.
     */
    QString title;

    /**
     * @brief The path to the game's executable file.
     */
    QString executablePath;

    /**
     * @brief The name of the game's executable file.
     */
    QString executableName;

    /**
     * @brief The Steam App ID of the game, if available.
     */
    std::optional<int> steamAppId;

    /**
     * @brief The path to the game's artwork, if available.
     */
    std::optional<QString> artworkPath;

    /**
     * @brief A flag indicating whether tracking is enabled for the game.
     */
    bool trackingEnabled{true};
};
} // namespace gamelog::core::domain
