//
// Created by kj on 8/14/26.
//

#include "AppPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace gamelog::core
{
    QString AppPaths::dataDirectory() { return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation); }

    QString AppPaths::databasePath() { return QDir{dataDirectory()}.filePath(QStringLiteral("gamelog.sqlite")); }

    QString AppPaths::artworkDirectory() { return QDir{dataDirectory()}.filePath(QStringLiteral("artwork")); }

    QString AppPaths::gameArtworkDirectory(int gameId)
    {
        return QDir{artworkDirectory()}.filePath(QString::number(gameId));
    }
}