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

    class GameArtworkService:public QObject
    {
        Q_OBJECT

    public:
        explicit GameArtworkService();

        ~GameArtworkService() override = default;

        /**
         * @brief Gets the artwork for a specific game using the available methods coded, starting with:  .
         * - Local files
         * - Steam Web API
         * @param game the game to get artwork for.
         * @return A boolean describing if the artwork was fetched.
         */
        bool getGameArtwork(const core::domain::Game& game);

        /**
         * Makes the artwork directory for the game inside the local file system.
         * @param gameId The ID of the game to make the directory for, which will be the name of the directory.
         * @return A boolean describing if the directory was constructed.
         */
        static bool makeGameArtworkDirectory(int gameId);

        static QString artworkTypeToString(ArtworkType artworkType);

        // bool installCustomArtwork();

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
