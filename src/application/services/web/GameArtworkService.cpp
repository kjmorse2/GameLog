//
// Created by kj on 8/14/26.
//

#include "GameArtworkService.h"

#include <ranges>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QIODevice>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <resources/AppPaths.h>

namespace gamelog::application::services
{
    namespace
    {
        QByteArray expectedImageFormat(ArtworkType artworkType)
        {
            switch(artworkType)
            {
            case ArtworkType::Cover:
            case ArtworkType::Header:
                return QByteArrayLiteral("JPG");
            case ArtworkType::Logo:
                return QByteArrayLiteral("PNG");
            }

            return {};
        }

        QString artworkExtension(ArtworkType artworkType)
        {
            switch(artworkType)
            {
            case ArtworkType::Cover:
            case ArtworkType::Header:
                return QStringLiteral("jpg");
            case ArtworkType::Logo:
                return QStringLiteral("png");
            }

            return {};
        }
    } // namespace

    GameArtworkService::GameArtworkService(QObject* parent)
        : QObject{parent},
          networkAccessManager_{new QNetworkAccessManager{this}}
    {
        connect(networkAccessManager_,
                &QNetworkAccessManager::finished,
                this,
                [this](QNetworkReply* reply)
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
                        emit artworkUnavailable(gameId, artworkType);
                    }

                    reply->deleteLater();
                });
    }

    GameArtworkService::GameArtworkService(QNetworkAccessManager& networkAccessManager, QObject* parent)
        : QObject{parent},
          networkAccessManager_{&networkAccessManager}
    {
        connect(networkAccessManager_,
                &QNetworkAccessManager::finished,
                this,
                [this](QNetworkReply* reply)
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
                        emit artworkUnavailable(gameId, artworkType);
                    }

                    reply->deleteLater();
                });
    }

    bool GameArtworkService::getGameArtwork(const core::domain::Game& game)
    {
        if(game.id <= 0) { return false; }

        const QString coverPath = QDir{core::AppPaths::gameArtworkDirectory(game.id)}.
            filePath(QStringLiteral("cover.jpg"));

        if(isValidArtworkFile(coverPath, ArtworkType::Cover))
        {
            if(!game.hasArtwork)
            {
                qWarning() << "Valid artwork found locally but not recorded in the database:" << coverPath;
            }

            emit artworkAvailable(game.id, ArtworkType::Cover);
            return true;
        }

        if(game.hasArtwork)
        {
            qWarning() << "Game is marked as having artwork, but cover.jpg is missing or invalid for game:" << game.id;
        }

        // The persisted flag describes current local cover availability, not a
        // pending request. Clear stale state before attempting a replacement.
        emit artworkUnavailable(game.id, ArtworkType::Cover);

        if(game.steamAppId && *game.steamAppId > 0 && getSteamArtwork(game))
        {
            qInfo() << "Artwork download initiated for game:" << game.id;
        }

        return false;
    }

    bool GameArtworkService::makeGameArtworkDirectory(int gameId)
    {
        if(gameId <= 0) { return false; }

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
        }

        return QStringLiteral("Unknown");
    }

    bool GameArtworkService::getSteamArtwork(const core::domain::Game& game) const
    {
        if(networkAccessManager_ == nullptr || game.id <= 0 || !game.steamAppId || *game.steamAppId <= 0)
        {
            return false;
        }

        for(const auto& type : ArtWorkTypeToSteamUrl | std::views::keys)
        {
            const QUrl next = makeSteamArtworkUrl(*game.steamAppId, type);
            QNetworkReply* reply = networkAccessManager_->get(QNetworkRequest{next});
            reply->setProperty("artworkType", static_cast<int>(type));
            reply->setProperty("gameId", game.id);
        }

        return true;
    }

    void GameArtworkService::parseSteamArtworkReply(QNetworkReply* reply, ArtworkType artworkType, int gameId)
    {
        const QByteArray data = reply->readAll();
        const QByteArray imageFormat = expectedImageFormat(artworkType);

        if(data.isEmpty() || imageFormat.isEmpty())
        {
            qWarning() << "Received empty or unsupported artwork response for" << artworkTypeToString(artworkType);
            emit artworkUnavailable(gameId, artworkType);
            return;
        }

        QImage decodedImage;
        if(!decodedImage.loadFromData(data, imageFormat.constData()))
        {
            qWarning() << "Artwork response did not decode as the expected" << imageFormat << "image for" <<
                artworkTypeToString(artworkType);
            emit artworkUnavailable(gameId, artworkType);
            return;
        }

        if(!makeGameArtworkDirectory(gameId))
        {
            emit artworkUnavailable(gameId, artworkType);
            return;
        }

        const QString extension = artworkExtension(artworkType);
        const QString fileName = QStringLiteral("%1.%2").arg(artworkTypeToString(artworkType), extension);
        const QString filePath = QDir{core::AppPaths::gameArtworkDirectory(gameId)}.filePath(fileName);

        QFile file{filePath};
        if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            qWarning() << "Failed to open artwork file:" << filePath << file.errorString();
            emit artworkUnavailable(gameId, artworkType);
            return;
        }

        if(file.write(data) != data.size() || !file.flush())
        {
            qWarning() << "Failed to write artwork file:" << filePath << file.errorString();
            file.close();
            file.remove();
            emit artworkUnavailable(gameId, artworkType);
            return;
        }

        file.close();
        emit artworkAvailable(gameId, artworkType);
    }

    bool GameArtworkService::isValidArtworkFile(const QString& filePath, ArtworkType artworkType)
    {
        const QFileInfo fileInfo{filePath};
        if(!fileInfo.isFile() || fileInfo.size() <= 0) { return false; }

        const QByteArray imageFormat = expectedImageFormat(artworkType);
        if(imageFormat.isEmpty()) { return false; }

        QImage image;
        return image.load(filePath, imageFormat.constData()) && !image.isNull();
    }

    QUrl GameArtworkService::makeSteamArtworkUrl(int steamAppId, ArtworkType artworkType)
    {
        return QUrl{
            QStringLiteral("https://cdn.cloudflare.steamstatic.com/steam/apps/%1/%2").arg(steamAppId).
            arg(ArtWorkTypeToSteamUrl.at(artworkType))
        };
    }

    const std::pmr::map<ArtworkType, QString> GameArtworkService::ArtWorkTypeToSteamUrl{
        {ArtworkType::Cover, QStringLiteral("library_600x900.jpg")},
        {ArtworkType::Header, QStringLiteral("header.jpg")}, {ArtworkType::Logo, QStringLiteral("logo.png")}
    };
} // namespace gamelog::application::services
