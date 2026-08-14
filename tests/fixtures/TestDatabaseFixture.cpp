#include "fixtures/TestDatabaseFixture.h"

#include <QUuid>

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace gamelog::tests::fixtures
{
    QString createFreshTestDatabasePath(const QString& namePrefix)
    {
        const QString baseDirectory = QDir::temp().filePath("gamelog-test-databases");
        QDir directory;

        if(!directory.mkpath(baseDirectory))
        {
            return {};
        }

        const QString databasePath = QDir{baseDirectory}.filePath(namePrefix + ".sqlite");

        cleanupDatabaseArtifacts(databasePath);
        return databasePath;
    }

    QString createUniqueConnectionName(const QString& namePrefix)
    {
        return namePrefix + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    void cleanupDatabaseArtifacts(const QString& databasePath)
    {
        QFile::remove(databasePath);
        QFile::remove(databasePath + "-wal");
        QFile::remove(databasePath + "-shm");
        QFile::remove(databasePath + "-journal");

        const QFileInfo fileInfo{databasePath};

        if(fileInfo.exists())
        {
            QFile::setPermissions(databasePath, QFile::permissions(databasePath) | QFileDevice::WriteUser);
            QFile::remove(databasePath);
        }
    }
} // namespace gamelog::tests::fixtures
