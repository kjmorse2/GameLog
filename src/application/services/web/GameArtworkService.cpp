#include "GameArtworkService.h"

#include "logging/LoggingCategories.h"

#include <array>

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

        /// File name each artwork type has on the Steam CDN.
        QString steamArtworkFileName(ArtworkType artworkType)
        {
            switch(artworkType)
            {
            case ArtworkType::Cover:
                return QStringLiteral("library_600x900.jpg");
            case ArtworkType::Header:
                return QStringLiteral("header.jpg");
            case ArtworkType::Logo:
                return QStringLiteral("logo.png");
            }

            return {};
        }

        /// Every artwork type requested when downloading a game's artwork.
        constexpr std::array kAllArtworkTypes{ArtworkType::Cover, ArtworkType::Header, ArtworkType::Logo};
    } // namespace

    GameArtworkService::GameArtworkService(QObject* parent)
        : QObject{parent},
          networkAccessManager_{new QNetworkAccessManager{this}}
    {
    }

    GameArtworkService::GameArtworkService(QNetworkAccessManager& networkAccessManager, QObject* parent)
        : QObject{parent},
          networkAccessManager_{&networkAccessManager}
    {
    }

    void GameArtworkService::onNetworkReplyFinished(QNetworkReply* reply, ArtworkType artworkType, int gameId)
    {
        if(reply->error() == QNetworkReply::NoError) { parseSteamArtworkReply(reply, artworkType, gameId); }
        else
        {
            qCWarning(gamelogArtworkServiceLog) << "Network error for" << artworkTypeToString(artworkType) << ":" <<
                reply->errorString();
            emit artworkUnavailable(gameId, artworkType);
        }

        reply->deleteLater();
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
                qCWarning(gamelogArtworkServiceLog) << "Valid artwork found locally but not recorded in the database:" << coverPath;
            }

            emit artworkAvailable(game.id, ArtworkType::Cover);
            return true;
        }

        if(game.hasArtwork)
        {
            qCWarning(gamelogArtworkServiceLog) << "Game is marked as having artwork, but cover.jpg is missing or invalid for game:" << game.id;
        }

        // The persisted flag describes current local cover availability, not a
        // pending request. Clear stale state before attempting a replacement.
        emit artworkUnavailable(game.id, ArtworkType::Cover);

        if(game.steamAppId && *game.steamAppId > 0 && getSteamArtwork(game))
        {
            qCInfo(gamelogArtworkServiceLog) << "Artwork download initiated for game:" << game.id;
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
            qCWarning(gamelogArtworkServiceLog) << "Failed to create artwork directory for game:" << gameId;
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

    bool GameArtworkService::getSteamArtwork(const core::domain::Game& game)
    {
        if(networkAccessManager_ == nullptr || game.id <= 0 || !game.steamAppId || *game.steamAppId <= 0)
        {
            return false;
        }

        const int gameId = game.id;

        for(const ArtworkType type : kAllArtworkTypes)
        {
            const QUrl next = makeSteamArtworkUrl(*game.steamAppId, type);
            QNetworkReply* reply = networkAccessManager_->get(QNetworkRequest{next});

            // Connect to this reply rather than to the manager's finished()
            // signal: an injected manager may be shared with other components,
            // whose replies must not be handled or deleted here.
            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, type, gameId] { onNetworkReplyFinished(reply, type, gameId); });
        }

        return true;
    }

    void GameArtworkService::parseSteamArtworkReply(QNetworkReply* reply, ArtworkType artworkType, int gameId)
    {
        const QByteArray data = reply->readAll();
        const QByteArray imageFormat = expectedImageFormat(artworkType);

        if(data.isEmpty() || imageFormat.isEmpty())
        {
            qCWarning(gamelogArtworkServiceLog) << "Received empty or unsupported artwork response for" << artworkTypeToString(artworkType);
            emit artworkUnavailable(gameId, artworkType);
            return;
        }

        QImage decodedImage;
        if(!decodedImage.loadFromData(data, imageFormat.constData()))
        {
            qCWarning(gamelogArtworkServiceLog) << "Artwork response did not decode as the expected" << imageFormat << "image for" <<
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
            qCWarning(gamelogArtworkServiceLog) << "Failed to open artwork file:" << filePath << file.errorString();
            emit artworkUnavailable(gameId, artworkType);
            return;
        }

        if(file.write(data) != data.size() || !file.flush())
        {
            qCWarning(gamelogArtworkServiceLog) << "Failed to write artwork file:" << filePath << file.errorString();
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
            arg(steamArtworkFileName(artworkType))
        };
    }

} // namespace gamelog::application::services
