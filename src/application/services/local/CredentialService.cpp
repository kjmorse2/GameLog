#include "application/services/local/CredentialService.h"

#include <logging/LoggingCategories.h>
#include <qt6keychain/keychain.h>

namespace gamelog::application::services
{
    namespace
    {
        bool isBlank(const QString& value) { return value.trimmed().isEmpty(); }
    } // namespace

    CredentialService::CredentialService(QObject* parent) : QObject{parent} {}

    void CredentialService::setSecret(const QString& key, const QString& secret)
    {
        qCDebug(gamelogSessionServiceLog) << "Setting secret for key";

        if(isBlank(key))
        {
            emit credentialError(key, QStringLiteral("Credential key cannot be empty or whitespace-only."));
            return;
        }
        if(isBlank(secret))
        {
            emit credentialError(key, QStringLiteral("Credential secret cannot be empty or whitespace-only."));
            return;
        }

        auto* job = createWritePasswordJob();
        if(job == nullptr)
        {
            emit credentialError(key, QStringLiteral("Unable to create credential write job."));
            return;
        }

        job->setKey(key);
        job->setTextData(secret);

        connect(job,
                &QKeychain::WritePasswordJob::finished,
                this,
                [this, job, key]
                {
                    const auto error = job->error();
                    const QString errorString = job->errorString();
                    job->deleteLater();

                    if(error != QKeychain::NoError)
                    {
                        emit credentialError(key, errorString);
                        return;
                    }

                    emit secretStored(key);
                });

        job->start();
    }

    void CredentialService::getSecret(const QString& key)
    {
        if(isBlank(key))
        {
            emit credentialError(key, QStringLiteral("Credential key cannot be empty or whitespace-only."));
            return;
        }

        auto* job = createReadPasswordJob();
        if(job == nullptr)
        {
            emit credentialError(key, QStringLiteral("Unable to create credential read job."));
            return;
        }

        job->setKey(key);

        connect(job,
                &QKeychain::ReadPasswordJob::finished,
                this,
                [this, job, key]
                {
                    const auto error = job->error();
                    const QString errorString = job->errorString();
                    const QString secret = job->textData();
                    job->deleteLater();

                    if(error == QKeychain::EntryNotFound)
                    {
                        emit secretNotFound(key);
                        return;
                    }

                    if(error != QKeychain::NoError)
                    {
                        emit credentialError(key, errorString);
                        return;
                    }

                    emit secretRetrieved(key, secret);
                });

        job->start();
    }

    void CredentialService::removeSecret(const QString& key)
    {
        if(isBlank(key))
        {
            emit credentialError(key, QStringLiteral("Credential key cannot be empty or whitespace-only."));
            return;
        }

        auto* job = createDeletePasswordJob();
        if(job == nullptr)
        {
            emit credentialError(key, QStringLiteral("Unable to create credential delete job."));
            return;
        }

        job->setKey(key);

        connect(job,
                &QKeychain::DeletePasswordJob::finished,
                this,
                [this, job, key]
                {
                    const auto error = job->error();
                    const QString errorString = job->errorString();
                    job->deleteLater();

                    if(error == QKeychain::EntryNotFound)
                    {
                        emit secretNotFound(key);
                        return;
                    }

                    if(error != QKeychain::NoError)
                    {
                        emit credentialError(key, errorString);
                        return;
                    }

                    emit secretRemoved(key);
                });

        job->start();
    }

    QKeychain::WritePasswordJob* CredentialService::createWritePasswordJob()
    {
        return new QKeychain::WritePasswordJob{QString::fromLatin1(kServiceName), this};
    }

    QKeychain::ReadPasswordJob* CredentialService::createReadPasswordJob()
    {
        return new QKeychain::ReadPasswordJob{QString::fromLatin1(kServiceName), this};
    }

    QKeychain::DeletePasswordJob* CredentialService::createDeletePasswordJob()
    {
        return new QKeychain::DeletePasswordJob{QString::fromLatin1(kServiceName), this};
    }
} // namespace gamelog::application::services
