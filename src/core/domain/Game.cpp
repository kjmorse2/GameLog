#include "Game.h"
#include <QDebugStateSaver>

namespace gamelog::core::domain
{
    QDebug operator<<(QDebug debug, const Game& game)
    {
        QDebugStateSaver saver{debug};

        debug.nospace() << "Game {" << "id: " << game.id << ", title: " << game.title << ", executablePath: " << game.
            executablePath << ", executableName: " << game.executableName << ", steamAppId: " << (
                game.steamAppId.has_value() ? QString::number(*game.steamAppId) : "nullopt") << ", hasArtwork: " << (
                game.hasArtwork) << ", trackingEnabled: " << game.trackingEnabled << "}";
        return debug;
    }
}
