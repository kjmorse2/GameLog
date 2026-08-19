#pragma once

#include <chrono>

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>

class QNetworkReply;

namespace gamelog::application::services
{
    class CredentialService;

    /**
     * Fetches the user's owned Steam library from the Steam Web API.
     *
     * A request is a two-stage operation: the API key and player ID are pulled
     * asynchronously from CredentialService, and only once both arrive is the
     * HTTP request sent. One request may be in flight at a time; a second call
     * fails rather than queueing. Every outcome is reported through
     * ownedGamesReceived() or requestFailed(), including the guard that fails a
     * request whose credential callbacks never arrive.
     *
     * The API key is passed only as the `key` query parameter, never as a
     * header, and neither it nor the full query string is ever logged.
     */
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

        /**
         * Overrides how long the service waits for CredentialService to answer
         * before failing the request. A narrow seam so tests need not wait the
         * production timeout; production callers use the default.
         * @param timeout The credential wait before the request is abandoned.
         */
        void setCredentialTimeout(std::chrono::milliseconds timeout);

    Q_SIGNALS:
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

    private Q_SLOTS:
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
         * Wires the CredentialService signals used to complete a pending request.
         * Shared by both constructors, which differ only in manager ownership.
         */
        void connectCredentialService();

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

        /**
         * Fails a request whose credential callbacks never arrive. Without it a
         * silent keychain would leave requestInProgress_ set for the lifetime of
         * the process and reject every later request. Covers only the credential
         * phase; once the HTTP request starts, the reply drives completion.
         */
        QTimer credentialTimeout_;

        /**
         * Default credential wait. Generous enough for a keychain that prompts
         * the user to unlock, short enough that a wedged request recovers.
         */
        static constexpr std::chrono::seconds kDefaultCredentialTimeout{30};
    };
} // namespace gamelog::application::services
