//
// Created by kj on 8/14/26.
//

#include "GameArtworkService.h"

#include <QStandardPaths>
#include <QDir>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <resources/AppPaths.h>

namespace gamelog::application::services
{
    GameArtworkService::GameArtworkService() : networkAccessManager_(new QNetworkAccessManager(this))
    {
        connect(networkAccessManager_,
                &QNetworkAccessManager::finished,
                this,
                [](QNetworkReply* reply)
                {
                    bool typeOk = false;
                    const int artworkTypeInt = reply->property("artworkType").toInt(&typeOk);

                    bool gameIdOk = false;
                    const int gameId = reply->property("gameId").toInt(&gameIdOk);

                    if(!typeOk || !gameIdOk)
                    {
                        qWarning() << "Missing artwork metadata for reply:" << reply->url();
                        reply->deleteLater();
                        return;
                    }

                    const auto artworkType = static_cast<ArtworkType>(artworkTypeInt);

                    if(reply->error() == QNetworkReply::NoError) { parseSteamArtworkReply(reply, artworkType, gameId); }
                    else
                    {
                        qWarning() << "Network error for" << artworkTypeToString(artworkType) << ":" << reply->
                            errorString();
                    }
                    reply->deleteLater();
                });
    }

    bool GameArtworkService::getGameArtwork(const core::domain::Game& game)
    {
        if(game.hasArtwork)
        {
            qDebug() << "Game already has artwork, skipping download for game:" << game.id;
            return true;
        }
        QDir artworkRoot{core::AppPaths::artworkDirectory()};

        const QString gameDirectoryName = QString::number(game.id);
        if(artworkRoot.cd(gameDirectoryName))
        {
            qWarning() << "Artwork found, but not logged in database" << artworkRoot.absolutePath();
            return true;
        }
        if(game.steamAppId.has_value() && getSteamArtwork(game))
        {
            qInfo() << "Artwork download initiated for game:" << game.id;
            return true;
        }

        return false;
    }

    bool GameArtworkService::makeGameArtworkDirectory(int gameId)
    {
        QDir artworkRoot{core::AppPaths::artworkDirectory()};

        const QString gameDirectoryName = QString::number(gameId);

        if(!artworkRoot.mkpath(gameDirectoryName))
        {
            qWarning() << "Failed to create artwork directory for game:" << gameId;
            return false;
        }
        return true;
    }

    QString GameArtworkService::artworkTypeToString(ArtworkType artworkType)
    {
        switch(artworkType)
        {
        case ArtworkType::Cover:
            return QStringLiteral("cover");
        case ArtworkType::Header:
            return QStringLiteral("header");
        case ArtworkType::Logo:
            return QStringLiteral("logo");
        default:
            return QStringLiteral("Unknown");
        }
    }

    bool GameArtworkService::getSteamArtwork(const core::domain::Game& game) const
    {
        for(const auto& type : ArtWorkTypeToSteamUrl | std::views::keys)
        {
            QUrl next = makeSteamArtworkUrl(game.steamAppId.value(), type);
            QNetworkReply* reply = networkAccessManager_->get(QNetworkRequest(next));
            reply->setProperty("artworkType", static_cast<int>(type));
            reply->setProperty("gameId", game.id);
        }
        return true;
    }

    void GameArtworkService::parseSteamArtworkReply(QNetworkReply* reply, ArtworkType artworkType, int gameId)
    {
        const QByteArray data = reply->readAll();

        if(data.isEmpty())
        {
            qWarning() << "Received empty artwork response for" << artworkTypeToString(artworkType);
            return;
        }

        QDir artworkRoot{core::AppPaths::artworkDirectory()};

        const QString gameDirectoryName = QString::number(gameId);

        if(!artworkRoot.mkpath(gameDirectoryName))
        {
            qWarning() << "Failed to create artwork directory for game:" << gameId;
            return;
        }

        QDir gameArtworkDirectory{artworkRoot.filePath(gameDirectoryName)};

        QString extension;

        switch(artworkType)
        {
        case ArtworkType::Cover:
        case ArtworkType::Header:
            extension = QStringLiteral("jpg");
            break;

        case ArtworkType::Logo:
            extension = QStringLiteral("png");
            break;

        default: qWarning() << "Unknown artwork type:" << static_cast<int>(artworkType);
            return;
        }

        const QString fileName = QStringLiteral("%1.%2").arg(artworkTypeToString(artworkType), extension);

        const QString filePath = gameArtworkDirectory.filePath(fileName);

        QFile file{filePath};

        if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            qWarning() << "Failed to open artwork file:" << filePath << file.errorString();
            return;
        }

        if(file.write(data) != data.size())
        {
            qWarning() << "Failed to write artwork file:" << filePath << file.errorString();
        }
    }

    QUrl GameArtworkService::makeSteamArtworkUrl(int steamAppId, ArtworkType artworkType)
    {
        return QUrl{
            QStringLiteral("https://cdn.cloudflare.steamstatic.com/steam/apps/%1/%2").arg(steamAppId).
            arg(ArtWorkTypeToSteamUrl.at(artworkType))
        };
    }

    const std::pmr::map<ArtworkType, QString> GameArtworkService::ArtWorkTypeToSteamUrl{
        {ArtworkType::Cover, "library_600x900.jpg"}, {ArtworkType::Header, "header.jpg"},
        {ArtworkType::Logo, "logo.png"},
    };
}
