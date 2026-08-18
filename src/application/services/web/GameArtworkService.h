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
        Cover, Header, Logo,
    };

    class GameArtworkService : public QObject
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

        /**
         * Converts the enum ArtworkType to a string representation.
         * @param artworkType The ArtworkType enum value to convert.
         * @return A QString representing the ArtworkType.
         */
        static QString artworkTypeToString(ArtworkType artworkType);

        // TODO bool installCustomArtwork();

        signals  :
        /**
         * Emitted when artwork for a specific game is available, either from local files or from the Steam Web API.
         * @param gameId The ID of the game for which artwork is available.
         */
        void artworkAvailable(int gameId);

        /**
         * Emitted when artwork for a specific game is unavailable, either from local files or from the Steam Web API.
         * @param gameId The ID of the game for which artwork is unavailable.
         */
        void artworkUnavailable(int gameId);

    private:
        /**
         * @brief The QNetworkAccessManager used for making network requests to the Steam Web API. This is used to fetch artwork for games that are not available locally.
         */
        QNetworkAccessManager* networkAccessManager_;

        /**
         * Gets the artwork for a specific game from the Steam Web API. This is used when local artwork is not available.
         * @param game The game to get artwork for.
         * @return A boolean describing if the artwork was fetched from the Steam Web API.
         */
        bool getSteamArtwork(const core::domain::Game& game) const;

        /**
         * Parses the reply from the Steam Web API for artwork requests. This is used to extract the artwork URLs from the JSON response and download the artwork images.
         * @param reply The QNetworkReply containing the JSON response from the Steam Web API.
         * @param artworkType The type of artwork being requested (cover, header, logo), used to name the file.
         * @param gameId The ID of the game for which artwork is being requested, used to name the directory.
         */
        static void parseSteamArtworkReply(QNetworkReply* reply, ArtworkType artworkType, int gameId);

        /**
         * Constructs a QUrl for the Steam Web API artwork endpoint based on the Steam App ID and the type of artwork requested. This is used to fetch artwork for games that are not available locally.
         * @param steamAppId Steam App ID of the game for which artwork is being requested.
         * @param artworkType The type of artwork being requested (cover, header, logo).
         * @return
         */
        static QUrl makeSteamArtworkUrl(int steamAppId, ArtworkType artworkType);

        /**
         * A static map that associates each ArtworkType with its corresponding Steam Web API URL. This is used to construct the correct URL for fetching artwork from the Steam Web API.
         */
        const static std::pmr::map<ArtworkType, QString> ArtWorkTypeToSteamUrl;
    };
}

#endif //GAMELOG_GAMEARTWORKSERVICE_H
