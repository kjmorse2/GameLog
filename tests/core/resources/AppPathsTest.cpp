#include <QtTest/QtTest>

#include "resources/AppPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

using gamelog::core::AppPaths;

namespace
{
    class AppPathsTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        static void dataDirectory_isNonEmpty();

        static void databasePath_livesUnderDataDirectoryAndNamesTheSqliteFile();

        static void artworkDirectory_livesUnderDataDirectory();

        static void gameArtworkDirectory_appendsGameIdToArtworkDirectory();

        static void gameArtworkDirectory_returnsDistinctPathsForDistinctIds();

        static void gameArtworkDirectory_documentsZeroAndNegativeIdBehavior();
    };
} // namespace

void AppPathsTest::init()
{
    // Keeps the assertions away from the developer's real data directory while
    // still exercising the same QStandardPaths resolution.
    QStandardPaths::setTestModeEnabled(true);
}

void AppPathsTest::dataDirectory_isNonEmpty()
{
    const QString dataDirectory = AppPaths::dataDirectory();

    QVERIFY(!dataDirectory.isEmpty());
    QCOMPARE(dataDirectory, QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
}

void AppPathsTest::databasePath_livesUnderDataDirectoryAndNamesTheSqliteFile()
{
    const QString dataDirectory = AppPaths::dataDirectory();
    const QString databasePath = AppPaths::databasePath();

    QVERIFY(databasePath.endsWith(QStringLiteral("gamelog.sqlite")));
    QCOMPARE(databasePath, QDir{dataDirectory}.filePath(QStringLiteral("gamelog.sqlite")));
    QCOMPARE(QFileInfo{databasePath}.absolutePath(), QDir{dataDirectory}.absolutePath());
}

void AppPathsTest::artworkDirectory_livesUnderDataDirectory()
{
    const QString dataDirectory = AppPaths::dataDirectory();
    const QString artworkDirectory = AppPaths::artworkDirectory();

    QCOMPARE(artworkDirectory, QDir{dataDirectory}.filePath(QStringLiteral("artwork")));
    QVERIFY(artworkDirectory.startsWith(dataDirectory));
    QVERIFY(artworkDirectory.endsWith(QStringLiteral("artwork")));
}

void AppPathsTest::gameArtworkDirectory_appendsGameIdToArtworkDirectory()
{
    const QString artworkDirectory = AppPaths::artworkDirectory();

    QCOMPARE(AppPaths::gameArtworkDirectory(1), QDir{artworkDirectory}.filePath(QStringLiteral("1")));
    QCOMPARE(AppPaths::gameArtworkDirectory(4211), QDir{artworkDirectory}.filePath(QStringLiteral("4211")));
}

void AppPathsTest::gameArtworkDirectory_returnsDistinctPathsForDistinctIds()
{
    QVERIFY(AppPaths::gameArtworkDirectory(1) != AppPaths::gameArtworkDirectory(2));
    QCOMPARE(AppPaths::gameArtworkDirectory(7), AppPaths::gameArtworkDirectory(7));
}

void AppPathsTest::gameArtworkDirectory_documentsZeroAndNegativeIdBehavior()
{
    // AppPaths performs no validation; callers such as GameArtworkService reject
    // non-positive IDs before ever asking for a directory.
    const QString artworkDirectory = AppPaths::artworkDirectory();

    QCOMPARE(AppPaths::gameArtworkDirectory(0), QDir{artworkDirectory}.filePath(QStringLiteral("0")));
    QCOMPARE(AppPaths::gameArtworkDirectory(-3), QDir{artworkDirectory}.filePath(QStringLiteral("-3")));
}

QTEST_GUILESS_MAIN(AppPathsTest)

#include "AppPathsTest.moc"
