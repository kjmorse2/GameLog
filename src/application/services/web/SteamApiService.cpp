#include "SteamApiService.h"

#include "application/services/local/CredentialService.h"
#include "logging/LoggingCategories.h"

#include <utility>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace gamelog::application::services
{
    SteamApiService::SteamApiService(CredentialService& credentialService, QObject* parent)
        : QObject{parent},
          credentialService_{credentialService},
          networkAccessManager_{new QNetworkAccessManager{this}}
    {
        connectCredentialService();
    }

    SteamApiService::SteamApiService(CredentialService& credentialService,
                                     QNetworkAccessManager& networkAccessManager,
                                     QObject* parent)
        : QObject{parent},
          credentialService_{credentialService},
          networkAccessManager_{&networkAccessManager}
    {
        connectCredentialService();
    }

    void SteamApiService::setCredentialTimeout(const std::chrono::milliseconds timeout)
    {
        credentialTimeout_.setInterval(timeout);
    }

    void SteamApiService::connectCredentialService()
    {
        credentialTimeout_.setSingleShot(true);
        credentialTimeout_.setInterval(kDefaultCredentialTimeout);
        credentialTimeout_.setParent(this);

        connect(&credentialTimeout_,
                &QTimer::timeout,
                this,
                [this]
                {
                    qCWarning(gamelogSteamApiServiceLog) <<
                        "Timed out waiting for Steam credentials; abandoning the request.";
                    failRequest(QStringLiteral("Timed out waiting for Steam credentials."));
                });

        connect(&credentialService_, &CredentialService::secretRetrieved, this, &SteamApiService::onSecretRetrieved);
        connect(&credentialService_, &CredentialService::secretNotFound, this, &SteamApiService::onSecretNotFound);
        connect(&credentialService_, &CredentialService::credentialError, this, &SteamApiService::onCredentialError);
    }

    void SteamApiService::getOwnedGames()
    {
        qCInfo(gamelogSteamApiServiceLog) << "SteamApiService::getOwnedGames requested.";

        if(requestInProgress_)
        {
            qCWarning(gamelogSteamApiServiceLog) <<
                "Ignoring Steam owned-games request because another request is already in progress.";

            emit requestFailed(QStringLiteral("A Steam owned-games request is already in progress."));
            return;
        }

        requestInProgress_ = true;
        networkRequestStarted_ = false;
        steamApiKey_.clear();
        steamPlayerId_.clear();

        qCDebug(gamelogSteamApiServiceLog) << "Requesting Steam API key and player ID from CredentialService.";

        credentialTimeout_.start();

        credentialService_.getSecret(QString::fromLatin1(CredentialService::kSteamApiKey));
        credentialService_.getSecret(QString::fromLatin1(CredentialService::kSteamPlayerIdKey));
    }

    void SteamApiService::onSecretRetrieved(const QString& key, const QString& secret)
    {
        if(!requestInProgress_ || networkRequestStarted_) { return; }

        const QString steamApiKey = QString::fromLatin1(CredentialService::kSteamApiKey);
        const QString steamPlayerIdKey = QString::fromLatin1(CredentialService::kSteamPlayerIdKey);
        const QString trimmedSecret = secret.trimmed();

        if(key == steamApiKey)
        {
            if(trimmedSecret.isEmpty())
            {
                failRequest(QStringLiteral("The configured Steam API key is empty or whitespace-only."));
                return;
            }

            steamApiKey_ = trimmedSecret;

            // Never print the API key itself. Length is enough to verify that
            // CredentialService returned a plausible non-empty value.
            qCDebug(gamelogSteamApiServiceLog) << "Retrieved Steam API key." << "Length:" << steamApiKey_.size();
        }
        else if(key == steamPlayerIdKey)
        {
            if(trimmedSecret.isEmpty())
            {
                failRequest(QStringLiteral("The configured Steam player ID is empty or whitespace-only."));
                return;
            }

            steamPlayerId_ = trimmedSecret;
            qCDebug(gamelogSteamApiServiceLog) << "Retrieved Steam player ID." << "Length:" << steamPlayerId_.size();
        }
        else
        {
            // CredentialService may be serving other consumers as well.
            return;
        }

        tryStartOwnedGamesRequest();
    }

    void SteamApiService::onSecretNotFound(const QString& key)
    {
        if(!requestInProgress_) { return; }

        const QString steamApiKey = QString::fromLatin1(CredentialService::kSteamApiKey);
        const QString steamPlayerIdKey = QString::fromLatin1(CredentialService::kSteamPlayerIdKey);

        if(key == steamApiKey) { failRequest(QStringLiteral("Steam API key has not been configured.")); }
        else if(key == steamPlayerIdKey) { failRequest(QStringLiteral("Steam player ID has not been configured.")); }
    }

    void SteamApiService::onCredentialError(const QString& key, const QString& error)
    {
        if(!requestInProgress_) { return; }

        const QString steamApiKey = QString::fromLatin1(CredentialService::kSteamApiKey);
        const QString steamPlayerIdKey = QString::fromLatin1(CredentialService::kSteamPlayerIdKey);

        if(key != steamApiKey && key != steamPlayerIdKey) { return; }

        failRequest(QStringLiteral("Failed to retrieve Steam credentials: %1").arg(error));
    }

    void SteamApiService::tryStartOwnedGamesRequest()
    {
        qCDebug(gamelogSteamApiServiceLog) << "Checking whether Steam owned-games request can start." <<
            "API key available:" << !steamApiKey_.isEmpty() << "Player ID available:" << !steamPlayerId_.isEmpty() <<
            "Network request already started:" << networkRequestStarted_;

        // CredentialService returns each secret asynchronously, so the first
        // completion normally reaches this method before the other is ready.
        if(steamApiKey_.isEmpty() || steamPlayerId_.isEmpty()) { return; }

        // A duplicated secretRetrieved signal must not create a second HTTP
        // stream for the same sync operation.
        if(networkRequestStarted_)
        {
            qCWarning(gamelogSteamApiServiceLog) <<
                "Steam owned-games HTTP request was already started; ignoring duplicate credential completion.";
            return;
        }

        bool steamIdValid = false;
        const qulonglong steamId = steamPlayerId_.toULongLong(&steamIdValid);

        if(!steamIdValid || steamId == 0)
        {
            failRequest(QStringLiteral("The configured Steam player ID is invalid."));
            return;
        }

        QUrl url{QStringLiteral("https://api.steampowered.com/IPlayerService/GetOwnedGames/v0001/")};
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("key"), steamApiKey_);
        query.addQueryItem(QStringLiteral("steamid"), QString::number(steamId));
        query.addQueryItem(QStringLiteral("include_appinfo"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("include_played_free_games"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
        url.setQuery(query);

        QNetworkRequest request{url};
        networkRequestStarted_ = true;
        credentialTimeout_.stop();

        qCInfo(gamelogSteamApiServiceLog) << "Sending Steam GetOwnedGames request." << "Steam ID:" << steamId <<
            "API key length:" << steamApiKey_.size() << "Endpoint:" << url.adjusted(QUrl::RemoveQuery).toString();

        QNetworkReply* reply = networkAccessManager_->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply] { handleOwnedGamesReply(reply); });
    }

    void SteamApiService::handleOwnedGamesReply(QNetworkReply* reply)
    {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = reply->readAll();

        qCInfo(gamelogSteamApiServiceLog) << "Received Steam GetOwnedGames reply." << "HTTP status:" << statusCode <<
            "Qt network error:" << static_cast<int>(reply->error()) << "Body bytes:" << responseBody.size();

        if(reply->error() != QNetworkReply::NoError)
        {
            constexpr qsizetype kMaxLoggedResponseBytes = 1024;
            const QByteArray responsePreview = responseBody.left(kMaxLoggedResponseBytes);
            const QString errorString = reply->errorString();

            qCWarning(gamelogSteamApiServiceLog) << "Steam API HTTP request failed." << "HTTP status:" << statusCode <<
                "Qt error:" << static_cast<int>(reply->error()) << "Error string:" << errorString << "Response preview:"
                << responsePreview;

            reply->deleteLater();
            failRequest(QStringLiteral("Steam Web API request failed with HTTP %1: %2").arg(statusCode).
                        arg(errorString));
            return;
        }

        reply->deleteLater();

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);

        if(parseError.error != QJsonParseError::NoError)
        {
            failRequest(QStringLiteral("Failed to parse Steam response: %1").arg(parseError.errorString()));
            return;
        }

        if(!document.isObject())
        {
            failRequest(QStringLiteral("Steam returned an invalid response root."));
            return;
        }

        const QJsonObject root = document.object();
        const QJsonValue responseValue = root.value(QStringLiteral("response"));
        if(!responseValue.isObject())
        {
            failRequest(QStringLiteral("Steam response is missing the response object."));
            return;
        }

        const QJsonObject response = responseValue.toObject();
        const QJsonValue gamesValue = response.value(QStringLiteral("games"));
        if(!gamesValue.isArray())
        {
            failRequest(QStringLiteral("Steam response is missing a valid games array."));
            return;
        }

        QJsonArray games = gamesValue.toArray();

        qCInfo(gamelogSteamApiServiceLog) << "Steam GetOwnedGames request succeeded." << "Games returned:" << games.
            size();

        resetRequestState();
        emit ownedGamesReceived(std::move(games));
    }

    void SteamApiService::failRequest(const QString& error)
    {
        qCWarning(gamelogSteamApiServiceLog) << "Steam API request failed:" << error;
        resetRequestState();
        emit requestFailed(error);
    }

    void SteamApiService::resetRequestState()
    {
        credentialTimeout_.stop();
        steamApiKey_.clear();
        steamPlayerId_.clear();
        requestInProgress_ = false;
        networkRequestStarted_ = false;
    }
} // namespace gamelog::application::services
