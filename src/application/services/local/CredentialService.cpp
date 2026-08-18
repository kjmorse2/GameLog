#include "application/services/local/CredentialService.h"

#include <qloggingcategory.h>
#include <logging/LoggingCategories.h>
#include <qt6keychain/keychain.h>

namespace gamelog::application::services
{
    CredentialService::CredentialService(QObject* parent) : QObject{parent} {}

    void CredentialService::setSecret(const QString& key, const QString& secret)
    {
        qCDebug(gamelogSessionServiceLog) << "Setting secret for key";
        if(key.isEmpty())
        {
            emit credentialError(key, QStringLiteral("Credential key cannot be empty."));
            return;
        }

        auto* job = new QKeychain::WritePasswordJob{QString::fromLatin1(kServiceName), this};

        job->setKey(key);
        job->setTextData(secret);

        connect(job,
                &QKeychain::WritePasswordJob::finished,
                this,
                [this, job, key]
                {
                    if(job->error() != QKeychain::NoError)
                    {
                        emit credentialError(key, job->errorString());
                        return;
                    }

                    emit secretStored(key);
                });

        job->start();
    }

    void CredentialService::getSecret(const QString& key)
    {
        if(key.isEmpty())
        {
            emit credentialError(key, QStringLiteral("Credential key cannot be empty."));
            return;
        }

        auto* job = new QKeychain::ReadPasswordJob{QString::fromLatin1(kServiceName), this};

        job->setKey(key);

        connect(job,
                &QKeychain::ReadPasswordJob::finished,
                this,
                [this, job, key]
                {
                    if(job->error() == QKeychain::EntryNotFound)
                    {
                        emit secretNotFound(key);
                        return;
                    }

                    if(job->error() != QKeychain::NoError)
                    {
                        emit credentialError(key, job->errorString());
                        return;
                    }

                    emit secretRetrieved(key, job->textData());
                });

        job->start();
    }

    void CredentialService::removeSecret(const QString& key)
    {
        if(key.isEmpty())
        {
            emit credentialError(key, QStringLiteral("Credential key cannot be empty."));
            return;
        }

        auto* job = new QKeychain::DeletePasswordJob{QString::fromLatin1(kServiceName), this};

        job->setKey(key);

        connect(job,
                &QKeychain::DeletePasswordJob::finished,
                this,
                [this, job, key]
                {
                    if(job->error() == QKeychain::EntryNotFound)
                    {
                        emit secretNotFound(key);
                        return;
                    }

                    if(job->error() != QKeychain::NoError)
                    {
                        emit credentialError(key, job->errorString());
                        return;
                    }

                    emit secretRemoved(key);
                });
        job->start();
    }
}
