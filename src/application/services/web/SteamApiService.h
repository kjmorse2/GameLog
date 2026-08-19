#pragma once

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;

namespace gamelog::application::services
{
    class CredentialService;

    class SteamApiService : public QObject
    {
        Q_OBJECT

    public:
        explicit SteamApiService(CredentialService& credentialService, QObject* parent = nullptr);

        /**
         * Creates a SteamApiService that uses a caller-provided network access
         * manager. The manager remains owned by the caller.
         * @param credentialService The service used to retrieve Steam credentials.
         * @param networkAccessManager The network manager used for HTTP requests.
         * @param parent The parent QObject for this service.
         */
        SteamApiService(CredentialService& credentialService,
                        QNetworkAccessManager& networkAccessManager,
                        QObject* parent = nullptr);

        /**
         * Requests the current user's owned Steam games.
         *
         * Credentials are retrieved asynchronously from CredentialService.
         * The returned JSON game records are emitted through
         * ownedGamesReceived().
         */
        void getOwnedGames();

        signals  :
        /**
         * Emitted when Steam successfully returns the user's game library.
         *
         * The array contains the raw game objects returned by Steam. An empty
         * array is a successful empty library. GameService is responsible for
         * converting these objects into Game domain objects.
         */
        void ownedGamesReceived(QJsonArray games);

        /**
         * Emitted if credentials are unavailable or invalid, the HTTP request
         * fails, or Steam returns a malformed response.
         */
        void requestFailed(QString error);

    private
        slots  :
        /**
         * Connected to CredentialService::secretRetrieved. Starts the HTTP request
         * when both required, nonblank credentials are available.
         * @param key The key of the retrieved secret. Should be either kSteamApiKey or kSteamPlayerIdKey.
         * @param secret The value of the retrieved secret.
         */
        void onSecretRetrieved(const QString& key, const QString& secret);

        /**
         * Connected to CredentialService::secretNotFound. Fails the request if either the Steam API key or player ID is missing.
         * @param key The key of the secret that was not found. Should be either kSteamApiKey or kSteamPlayerIdKey.
         */
        void onSecretNotFound(const QString& key);

        /**
         * Connected to CredentialService::credentialError. Fails the request if there is an error retrieving either the Steam API key or player ID.
         * @param key The key of the secret that caused the error. Should be either kSteamApiKey or kSteamPlayerIdKey.
         * @param error The error message describing the failure to retrieve the secret.
         */
        void onCredentialError(const QString& key, const QString& error);

        /**
         * @brief Starts the HTTP request to Steam's GetOwnedGames API if both the API key and player ID are available.
         */
        void tryStartOwnedGamesRequest();

        /**
         * @brief Handles the HTTP reply from Steam's GetOwnedGames API. Parses the JSON response and emits ownedGamesReceived() or requestFailed() as appropriate.
         * @param reply The QNetworkReply containing the HTTP response from Steam.
         */
        void handleOwnedGamesReply(QNetworkReply* reply);

    private:
        /**
         * @brief Fails the current request and emits requestFailed() with the provided error message.
         * @param error The error message describing why the request failed.
         */
        void failRequest(const QString& error);

        /**
         * @brief Resets the internal state of the SteamApiService to allow for a new request to be made. This is called after a request completes or fails.
         */
        void resetRequestState();

        /**
         * The CredentialService used to retrieve the Steam API key and player ID. This service is responsible for securely storing and providing these credentials when requested.
         */
        CredentialService& credentialService_;

        /**
         * The QNetworkAccessManager used to perform HTTP requests to the Steam Web API. The service owns the default manager, while an injected manager remains caller-owned.
         */
        QNetworkAccessManager* networkAccessManager_{};

        /**
         * The Steam API key retrieved from the CredentialService. This key is required to authenticate requests to the Steam Web API.
         */
        QString steamApiKey_;

        /**
         * The Steam player ID retrieved from the CredentialService. This ID is required to identify the user when requesting their owned games from the Steam Web API.
         */
        QString steamPlayerId_;

        /**
         * Indicates whether a request for owned games is currently in progress. This flag prevents multiple simultaneous requests and ensures that only one request is active at a time.
         */
        bool requestInProgress_{false};

        /**
         * Protects against duplicate secretRetrieved signals starting more than
         * one HTTP request for a single sync operation.
         */
        bool networkRequestStarted_{false};
    };
} // namespace gamelog::application::services
