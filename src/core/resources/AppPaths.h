//
// Created by kj on 8/14/26.
//

#ifndef GAMELOG_APPPATHS_H
#define GAMELOG_APPPATHS_H

#include <QString>

namespace gamelog::core {

    class AppPaths
    {
    public:
        [[nodiscard]] static QString dataDirectory();
        [[nodiscard]] static QString databasePath();
        [[nodiscard]] static QString artworkDirectory();
        [[nodiscard]] static QString gameArtworkDirectory(int gameId);
    };

}
#endif //GAMELOG_APPPATHS_H
