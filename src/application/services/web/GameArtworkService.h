//
// Created by kj on 8/14/26.
//

#ifndef GAMELOG_GAMEARTWORKSERVICE_H
#define GAMELOG_GAMEARTWORKSERVICE_H

#include <map>

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QtNetwork/QNetworkAccessManager>

#include "domain/Game.h"

class QNetworkReply;

namespace gamelog::application::services
{
    enum class ArtworkType
    {
        Cover, Header, Logo
    };

    class GameArtworkService : public QObject
    {
        Q_OBJECT

    public:
        explicit GameArtworkService(QObject* parent = nullptr);

        /**
         * Creates a GameArtworkService using a caller-provided network manager.
         * The manager remains owned by the caller.
         * @param networkAccessManager The manager used for Steam artwork requests.
         * @param parent The parent QObject for this service.
         */
        explicit GameArtworkService(QNetworkAccessManager& networkAccessManager, QObject* parent = nullptr);

        ~GameArtworkService() override = default;

        /**
         * @brief Gets the artwork for a specific game using the available methods coded, starting with:
         * - Local files
         * - Steam CDN downloads
         *
         * The current completeness contract is based on a valid cover.jpg.
         * @param game the game to get artwork for.
         * @return True only when usable cover artwork exists locally when this method returns. Queueing downloads returns false.
         */
        [[nodiscard]] bool getGameArtwork(const core::domain::Game& game);

        /**
         * Makes the artwork directory for the game inside the local file system.
         * @param gameId The ID of the game to make the directory for, which will be the name of the directory.
         * @return A boolean describing if the directory was constructed.
         */
        [[nodiscard]] static bool makeGameArtworkDirectory(int gameId);

        /**
         * Converts the enum ArtworkType to a string representation.
         * @param artworkType The ArtworkType enum value to convert.
         * @return A QString representing the ArtworkType.
         */
        [[nodiscard]] static QString artworkTypeToString(ArtworkType artworkType);

        // TODO bool installCustomArtwork();

        signals  :
        /**
         * Emitted when one validated artwork file for a specific game is available,
         * either from local files or after a successful Steam download.
         * @param gameId The ID of the game for which artwork is available.
         * @param artworkType The specific artwork file that is available.
         */
        void artworkAvailable(int gameId, ArtworkType artworkType);

        /**
         * Emitted when one artwork request for a specific game is unavailable or invalid.
         * @param gameId The ID of the game for which artwork is unavailable.
         * @param artworkType The specific artwork file that is unavailable.
         */
        void artworkUnavailable(int gameId, ArtworkType artworkType);

    private:
        /**
         * @brief The QNetworkAccessManager used for making network requests to the Steam CDN. The default manager is service-owned; an injected manager is caller-owned.
         */
        QNetworkAccessManager* networkAccessManager_{};

        /**
         * Gets the artwork for a specific game from the Steam CDN. This is used when local artwork is not available.
         * @param game The game to get artwork for.
         * @return True if the download requests were queued.
         */
        [[nodiscard]] bool getSteamArtwork(const core::domain::Game& game) const;

        /**
         * Parses the reply from the Steam CDN for artwork requests, validates
         * the expected image format, writes the file, and emits the result.
         * @param reply The QNetworkReply containing the artwork bytes.
         * @param artworkType The type of artwork being requested (cover, header, logo), used to name the file.
         * @param gameId The ID of the game for which artwork is being requested, used to name the directory.
         */
        void parseSteamArtworkReply(QNetworkReply* reply, ArtworkType artworkType, int gameId);

        /**
         * Checks whether an existing artwork file decodes as the expected image format.
         * @param filePath The local artwork path.
         * @param artworkType The expected artwork type and file format.
         * @return True if the file is nonempty and decodes successfully.
         */
        [[nodiscard]] static bool isValidArtworkFile(const QString& filePath, ArtworkType artworkType);

        /**
         * Constructs a QUrl for the Steam CDN artwork endpoint based on the Steam App ID and the type of artwork requested. This is used to fetch artwork for games that are not available locally.
         * @param steamAppId Steam App ID of the game for which artwork is being requested.
         * @param artworkType The type of artwork being requested (cover, header, logo).
         * @return The Steam CDN URL.
         */
        [[nodiscard]] static QUrl makeSteamArtworkUrl(int steamAppId, ArtworkType artworkType);

        /**
         * A static map that associates each ArtworkType with its corresponding Steam CDN file. This is used to construct the correct URL for fetching artwork.
         */
        const static std::pmr::map<ArtworkType, QString> ArtWorkTypeToSteamUrl;
    };
} // namespace gamelog::application::services

Q_DECLARE_METATYPE(gamelog::application::services::ArtworkType)

#endif // GAMELOG_GAMEARTWORKSERVICE_H
