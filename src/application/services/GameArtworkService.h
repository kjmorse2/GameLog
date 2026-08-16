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

    class GameArtworkService : public QObject
    {
    Q_OBJECT
    public:
        explicit GameArtworkService();
        ~GameArtworkService() override = default;
        bool getGameArtwork(const core::domain::Game& game);
        static bool makeGameArtworkDirectory(int gameId) ;
        static QString artworkTypeToString(ArtworkType artworkType);
        // GameArtworkService
        bool installCustomArtwork();

        signals:
            void artworkAvailable(int gameId);
            void artworkUnavailable(int gameId);
    private:
        QNetworkAccessManager* networkAccessManager_;
        bool getSteamArtwork(const core::domain::Game& game) const;
        static void parseSteamArtworkReply(QNetworkReply* reply, ArtworkType artworkType, int gameId);
        static QUrl makeSteamArtworkUrl(int steamAppId, ArtworkType artworkType);
        const static std::pmr::map<ArtworkType, QString> ArtWorkTypeToSteamUrl;
    };
}

#endif //GAMELOG_GAMEARTWORKSERVICE_H
