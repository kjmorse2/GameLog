#pragma once

#include <QObject>
#include <QString>

namespace gamelog::application::services
{
    class CredentialService :public QObject
    {
        Q_OBJECT

    public:
        static constexpr auto kSteamApiKey = "steam_api_key";
        static constexpr auto kSteamPlayerIdKey = "player_id_key";

        explicit CredentialService(QObject *parent = nullptr);

        /**
         * Stores a secret under the given key.
         *
         * This is also used to replace an existing secret.
         */
        void setSecret(const QString &key, const QString &secret);

        /**
         * Asynchronously retrieves a secret.
         *
         * The result is returned through secretRetrieved().
         */
        void getSecret(const QString &key);

        /**
         * Removes a secret from the keychain.
         */
        void removeSecret(const QString &key);

    signals:
        void secretStored(const QString &key);

        void secretRetrieved(
                const QString &key,
                const QString &secret
                );

        void secretRemoved(const QString &key);

        void secretNotFound(const QString &key);

        void credentialError(
                const QString &key,
                const QString &error
                );

    private:
        static constexpr auto kServiceName = "GameLog";
    };
}
