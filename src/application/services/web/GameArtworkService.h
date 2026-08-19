#pragma once


#include <QMetaType>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QtNetwork/QNetworkAccessManager>

#include "domain/Game.h"

class QNetworkReply;

namespace gamelog::application::services
{
    /**
     * The artwork images GameLog stores per game. Each maps to one fixed file
     * name on disk and one Steam CDN file name.
     */
    enum class ArtworkType
    {
        Cover, Header, Logo
    };

    /**
     * Supplies local cover/header/logo artwork for a game, downloading it from
     * the Steam CDN when it is missing.
     *
     * Artwork lives on disk under AppPaths::gameArtworkDirectory(gameId), not in
     * the database; Game::hasArtwork only records whether a valid cover.jpg was
     * present. Downloads are asynchronous, so getGameArtwork() returns true only
     * when a usable cover already exists at the moment of the call - queueing
     * downloads returns false, and callers learn the outcome from
     * artworkAvailable()/artworkUnavailable(). Every downloaded file is decoded
     * before being written, so a Steam error page is never stored as an image.
     */
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

    Q_SIGNALS:
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
        [[nodiscard]] bool getSteamArtwork(const core::domain::Game& game);

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
         * Routes one finished artwork reply to parsing or to artworkUnavailable,
         * then schedules the reply for deletion. Connected per reply, so replies
         * issued by other users of an injected manager are never touched here.
         */
        void onNetworkReplyFinished(QNetworkReply* reply, ArtworkType artworkType, int gameId);

        /**
         * A static map that associates each ArtworkType with its corresponding Steam CDN file. This is used to construct the correct URL for fetching artwork.
         */
    };
} // namespace gamelog::application::services

Q_DECLARE_METATYPE(gamelog::application::services::ArtworkType)
