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
        explicit SteamApiService(
                CredentialService& credentialService,
                QObject* parent = nullptr
                );

        /**
         * Requests the current user's owned Steam games.
         *
         * Credentials are retrieved asynchronously from CredentialService.
         * The returned JSON game records are emitted through
         * ownedGamesReceived().
         */
        void getOwnedGames();

    signals:
        /**
         * Emitted when Steam successfully returns the user's game library.
         *
         * The array contains the raw game objects returned by Steam.
         * GameService is responsible for converting these objects into
         * Game domain objects.
         */
        void ownedGamesReceived(QJsonArray games);

        /**
         * Emitted if credentials are unavailable, the HTTP request fails,
         * or Steam returns an invalid response.
         */
        void requestFailed(QString error);

    private:
        void onSecretRetrieved(
                const QString& key,
                const QString& secret
                );

        void onSecretNotFound(const QString& key);

        void onCredentialError(
                const QString& key,
                const QString& error
                );

        void tryStartOwnedGamesRequest();

        void handleOwnedGamesReply(QNetworkReply* reply);

        void failRequest(const QString& error);

        void resetRequestState();

        CredentialService& credentialService_;

        QNetworkAccessManager networkAccessManager_;

        QString steamApiKey_;
        QString steamPlayerId_;

        bool requestInProgress_{false};

        /**
         * Protects against duplicate secretRetrieved signals starting more than
         * one HTTP request for a single sync operation.
         */
        bool networkRequestStarted_{false};
    };
} // namespace gamelog::application::services
