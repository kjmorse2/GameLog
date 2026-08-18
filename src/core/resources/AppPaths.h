//
// Created by kj on 8/14/26.
//

#ifndef GAMELOG_APPPATHS_H
#define GAMELOG_APPPATHS_H

#include <QString>

namespace gamelog::core
{
    class AppPaths
    {
    public:
        /**
         * The top level directory where the application stores its data, including the database and artwork.
         * @return The path to the data directory as a QString.
         */
        [[nodiscard]] static QString dataDirectory();

        /**
         * The path to the SQLite database file used by the application. This path is constructed based on the data directory.
         * @return The path to the database file as a QString.
         */
        [[nodiscard]] static QString databasePath();

        /**
         * The directory where the application stores artwork for games. This directory is created inside the data directory.
         * @return The path to the artwork directory as a QString.
         */
        [[nodiscard]] static QString artworkDirectory();

        /**
         * The directory where the application stores artwork for a specific game, identified by its game ID.
         * This directory is created inside the artwork directory.
         * @param gameId The ID of the game for which to get the artwork directory.
         * @return The path to the game's artwork directory as a QString.
         */
        [[nodiscard]] static QString gameArtworkDirectory(int gameId);
    };
}
#endif //GAMELOG_APPPATHS_H
