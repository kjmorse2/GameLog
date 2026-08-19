#pragma once

#include <QObject>
#include <QString>

namespace QKeychain
{
    class DeletePasswordJob;
    class ReadPasswordJob;
    class WritePasswordJob;
}

namespace gamelog::application::services
{
    class CredentialService : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief The key used to store the Steam API key in the keychain.
         */
        static constexpr auto kSteamApiKey = "steam_api_key";

        /**
         * @brief The key used to store the Steam Player ID in the keychain.
         */
        static constexpr auto kSteamPlayerIdKey = "player_id_key";

        /**
         * @brief The key used to store the Steam Player Name in the keychain.
         * @param parent The parent QObject for this service.
         */
        explicit CredentialService(QObject* parent = nullptr);

        ~CredentialService() override = default;

        /**
         * Stores a secret under the given key.
         *
         * This is also used to replace an existing secret. Empty or
         * whitespace-only keys and secrets are rejected; removal must be
         * requested explicitly through removeSecret().
         */
        void setSecret(const QString& key, const QString& secret);

        /**
         * Asynchronously retrieves a secret.
         *
         * Empty or whitespace-only keys are rejected. The result is returned
         * through secretRetrieved().
         */
        void getSecret(const QString& key);

        /**
         * Removes a secret from the keychain.
         *
         * Empty or whitespace-only keys are rejected.
         */
        void removeSecret(const QString& key);

        signals  :
        /**
         * Emitted when a secret has been successfully stored.
         * @param key the key under which the secret was stored
         */
        void secretStored(const QString& key);

        /**
         * Emitted when a secret has been successfully retrieved.
         * @param key The key under which the secret was stored
         * @param secret The secret that was retrieved
         */
        void secretRetrieved(const QString& key, const QString& secret);

        /**
         * Emitted when a secret has been successfully removed.
         * @param key The key under which the secret was stored
         */
        void secretRemoved(const QString& key);

        /**
         * Emitted when a secret could not be found for the given key.
         * @param key The key under which the secret was expected to be stored
         */
        void secretNotFound(const QString& key);

        /**
         * Emitted when an error occurs while storing, retrieving, or removing a secret.
         * @param key The key under which the secret was expected to be stored
         * @param error The error message describing the failure
         */
        void credentialError(const QString& key, const QString& error);

    protected:
        /**
         * Creates the concrete keychain write job. Tests may override this
         * narrow seam without changing production keychain behavior.
         */
        virtual QKeychain::WritePasswordJob* createWritePasswordJob();

        /**
         * Creates the concrete keychain read job. Tests may override this
         * narrow seam without changing production keychain behavior.
         */
        virtual QKeychain::ReadPasswordJob* createReadPasswordJob();

        /**
         * Creates the concrete keychain delete job. Tests may override this
         * narrow seam without changing production keychain behavior.
         */
        virtual QKeychain::DeletePasswordJob* createDeletePasswordJob();

    private:
        /**
         * @brief The name of the service used for storing credentials in the keychain.
         */
        static constexpr auto kServiceName = "GameLog";
    };
} // namespace gamelog::application::services
