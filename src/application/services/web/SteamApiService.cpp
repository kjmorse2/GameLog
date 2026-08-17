#include "SteamApiService.h"

#include "application/services/local/CredentialService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include "logging/LoggingCategories.h"

namespace gamelog::application::services
{
    SteamApiService::SteamApiService(
            CredentialService &credentialService,
            QObject *parent
            ) :
        QObject{parent},
        credentialService_{credentialService}
    {
        connect(
                &credentialService_,
                &CredentialService::secretRetrieved,
                this,
                &SteamApiService::onSecretRetrieved
                );

        connect(
                &credentialService_,
                &CredentialService::secretNotFound,
                this,
                &SteamApiService::onSecretNotFound
                );

        connect(
                &credentialService_,
                &CredentialService::credentialError,
                this,
                &SteamApiService::onCredentialError
                );
    }

    void SteamApiService::getOwnedGames()
    {
        qCInfo(gamelogSteamApiServiceLog)
            << "SteamApiService::getOwnedGames requested.";

        if (requestInProgress_)
        {
            qCWarning(gamelogSteamApiServiceLog)
                << "Ignoring Steam owned-games request because another request is already in progress.";

            emit requestFailed(
                    QStringLiteral(
                            "A Steam owned-games request is already in progress."
                            )
                    );
            return;
        }

        requestInProgress_ = true;
        networkRequestStarted_ = false;

        steamApiKey_.clear();
        steamPlayerId_.clear();

        qCDebug(gamelogSteamApiServiceLog)
            << "Requesting Steam API key and player ID from CredentialService.";

        credentialService_.getSecret(
                QString::fromLatin1(CredentialService::kSteamApiKey)
                );

        credentialService_.getSecret(
                QString::fromLatin1(CredentialService::kSteamPlayerIdKey)
                );
    }

    void SteamApiService::onSecretRetrieved(
            const QString &key,
            const QString &secret
            )
    {
        if (!requestInProgress_)
        {
            return;
        }

        const QString steamApiKey =
                QString::fromLatin1(CredentialService::kSteamApiKey);

        const QString steamPlayerIdKey =
                QString::fromLatin1(CredentialService::kSteamPlayerIdKey);

        if (key == steamApiKey)
        {
            steamApiKey_ = secret.trimmed();

            // Never print the API key itself. Length is enough to verify that
            // CredentialService returned a plausible non-empty value.
            qCDebug(gamelogSteamApiServiceLog)
                << "Retrieved Steam API key."
                << "Length:" << steamApiKey_.size();
        }
        else if (key == steamPlayerIdKey)
        {
            steamPlayerId_ = secret.trimmed();

            qCDebug(gamelogSteamApiServiceLog)
                << "Retrieved Steam player ID."
                << "Length:" << steamPlayerId_.size();
        }
        else
        {
            // CredentialService may be serving other consumers as well.
            return;
        }

        tryStartOwnedGamesRequest();
    }

    void SteamApiService::onSecretNotFound(const QString &key)
    {
        if (!requestInProgress_)
        {
            return;
        }

        const QString steamApiKey =
                QString::fromLatin1(CredentialService::kSteamApiKey);

        const QString steamPlayerIdKey =
                QString::fromLatin1(CredentialService::kSteamPlayerIdKey);

        if (key == steamApiKey)
        {
            failRequest(
                    QStringLiteral(
                            "Steam API key has not been configured."
                            )
                    );
        }
        else if (key == steamPlayerIdKey)
        {
            failRequest(
                    QStringLiteral(
                            "Steam player ID has not been configured."
                            )
                    );
        }
    }

    void SteamApiService::onCredentialError(
            const QString &key,
            const QString &error
            )
    {
        if (!requestInProgress_)
        {
            return;
        }

        const QString steamApiKey =
                QString::fromLatin1(CredentialService::kSteamApiKey);

        const QString steamPlayerIdKey =
                QString::fromLatin1(CredentialService::kSteamPlayerIdKey);

        if (key != steamApiKey && key != steamPlayerIdKey)
        {
            return;
        }

        failRequest(
                QStringLiteral(
                        "Failed to retrieve Steam credentials: %1"
                        ).arg(error)
                );
    }

    void SteamApiService::tryStartOwnedGamesRequest()
    {
        qCDebug(gamelogSteamApiServiceLog)
            << "Checking whether Steam owned-games request can start."
            << "API key available:" << !steamApiKey_.isEmpty()
            << "Player ID available:" << !steamPlayerId_.isEmpty()
            << "Network request already started:" << networkRequestStarted_;

        // CredentialService returns each secret asynchronously, so the first
        // completion normally reaches this method before the other is ready.
        if (steamApiKey_.isEmpty() || steamPlayerId_.isEmpty())
        {
            return;
        }

        // A duplicated secretRetrieved signal must not create a second HTTP
        // stream for the same sync operation.
        if (networkRequestStarted_)
        {
            qCWarning(gamelogSteamApiServiceLog) << "Steam owned-games HTTP request was already started; ignoring duplicate credential completion.";
            return;
        }

        bool steamIdValid = false;

        const qulonglong steamId = steamPlayerId_.toULongLong(&steamIdValid);

        if (!steamIdValid || steamId == 0)
        {
            failRequest(QStringLiteral("The configured Steam player ID is invalid."));
            return;
        }

        QUrl url{
                QStringLiteral(
                        "https://api.steampowered.com/"
                        "IPlayerService/"
                        "GetOwnedGames/"
                        "v0001/"
                        )
        };

        QUrlQuery query;


        query.addQueryItem(
                QStringLiteral("key"),
                steamApiKey_
                );

        query.addQueryItem(
                QStringLiteral("steamid"),
                QString::number(steamId)
                );

        query.addQueryItem(
                QStringLiteral("include_appinfo"),
                QStringLiteral("true")
                );

        query.addQueryItem(
                QStringLiteral("include_played_free_games"),
                QStringLiteral("true")
                );

        query.addQueryItem(
                QStringLiteral("format"),
                QStringLiteral("json")
                );

        url.setQuery(query);

        QNetworkRequest request{url};

        // Valve documents x-webapi-key as an alternative to the key query
        // parameter. Keeping it in the header prevents accidental URL logging.
        request.setRawHeader(
                QByteArrayLiteral("x-webapi-key"),
                steamApiKey_.toUtf8()
                );

        networkRequestStarted_ = true;

        qCInfo(gamelogSteamApiServiceLog)
            << "Sending Steam GetOwnedGames request."
            << "Steam ID:" << steamId
            << "API key length:" << steamApiKey_.size()
            << "URL:" << url.toString();

        QNetworkReply *reply =
                networkAccessManager_.get(request);

        connect(
                reply,
                &QNetworkReply::finished,
                this,
                [this, reply] {
                    handleOwnedGamesReply(reply);
                }
                );
    }

    void SteamApiService::handleOwnedGamesReply(QNetworkReply *reply)
    {
        const int statusCode =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        const QByteArray responseBody = reply->readAll();

        qCInfo(gamelogSteamApiServiceLog)
            << "Received Steam GetOwnedGames reply."
            << "HTTP status:" << statusCode
            << "Qt network error:" << static_cast<int>(reply->error())
            << "Body bytes:" << responseBody.size();

        if (reply->error() != QNetworkReply::NoError)
        {
            // Limit response logging so an unexpected server response cannot
            // flood the log. The request URL does not contain the API key.
            constexpr qsizetype kMaxLoggedResponseBytes = 1024;
            const QByteArray responsePreview =
                    responseBody.left(kMaxLoggedResponseBytes);

            qCWarning(gamelogSteamApiServiceLog)
                << "Steam API HTTP request failed."
                << "HTTP status:" << statusCode
                << "Qt error:" << static_cast<int>(reply->error())
                << "Error string:" << reply->errorString()
                << "Response preview:" << responsePreview;

            const QString error =
                    QStringLiteral(
                            "Steam Web API request failed with HTTP %1: %2"
                            )
                   .arg(statusCode)
                   .arg(reply->errorString());

            reply->deleteLater();
            failRequest(error);
            return;
        }

        reply->deleteLater();

        QJsonParseError parseError;

        const QJsonDocument document =
                QJsonDocument::fromJson(
                        responseBody,
                        &parseError
                        );

        if (parseError.error != QJsonParseError::NoError)
        {
            failRequest(
                    QStringLiteral(
                            "Failed to parse Steam response: %1"
                            ).arg(parseError.errorString())
                    );
            return;
        }

        if (!document.isObject())
        {
            failRequest(
                    QStringLiteral(
                            "Steam returned an invalid response."
                            )
                    );
            return;
        }

        const QJsonObject root = document.object();

        const QJsonObject response =
                root.value(
                        QStringLiteral("response")
                        ).toObject();

        if (response.isEmpty())
        {
            failRequest(
                    QStringLiteral(
                            "Steam returned no library information. "
                            "The user's game details may be private."
                            )
                    );
            return;
        }

        const QJsonValue gamesValue =
                response.value(
                        QStringLiteral("games")
                        );

        QJsonArray games;

        if (gamesValue.isArray())
        {
            games = gamesValue.toArray();
        }

        qCInfo(gamelogSteamApiServiceLog)
            << "Steam GetOwnedGames request succeeded."
            << "Games returned:" << games.size();

        resetRequestState();

        emit ownedGamesReceived(std::move(games));
    }

    void SteamApiService::failRequest(const QString &error)
    {
        qCWarning(gamelogSteamApiServiceLog)
            << "Steam API request failed:" << error;

        resetRequestState();

        emit requestFailed(error);
    }

    void SteamApiService::resetRequestState()
    {
        steamApiKey_.clear();
        steamPlayerId_.clear();

        requestInProgress_ = false;
        networkRequestStarted_ = false;
    }
} // namespace gamelog::application::services
