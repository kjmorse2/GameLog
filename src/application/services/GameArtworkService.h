//
// Created by kj on 8/14/26.
//

#ifndef GAMELOG_GAMEARTWORKSERVICE_H
#define GAMELOG_GAMEARTWORKSERVICE_H

#include <QString>
#include <QUrl>
#include <map>
#include <QtNetwork/QNetworkAccessManager>

#include "domain/Game.h"

class QNetworkReply;

namespace gamelog::application::services
{
    enum class ArtworkType
    {
        Cover,
        Header,
        Logo,
    };

    class GameArtworkService : QObject
    {
    Q_OBJECT
    public:
        explicit GameArtworkService();
        ~GameArtworkService() override = default;
        bool getGameArtwork(core::domain::Game& game);
        static bool makeGameArtworkDirectory(int gameId) ;
        static QString artworkTypeToString(ArtworkType artworkType);

    private:
        QNetworkAccessManager* networkAccessManager_;
        bool getSteamArtwork(core::domain::Game& game);
        static void parseSteamArtworkReply(QNetworkReply* reply, ArtworkType artworkType, int gameId);
        static QUrl makeSteamArtworkUrl(int steamAppId, ArtworkType artworkType);
        const static std::pmr::map<ArtworkType, QString> ArtWorkTypeToSteamUrl;
    };
}

#endif //GAMELOG_GAMEARTWORKSERVICE_H
