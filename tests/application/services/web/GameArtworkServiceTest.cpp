#include <QtTest/QtTest>

#include "application/services/web/GameArtworkService.h"
#include "domain/Game.h"
#include "fixtures/FakeNetworkAccessManager.h"
#include "fixtures/LoggingTestSupport.h"
#include "resources/AppPaths.h"

#include <memory>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QStandardPaths>

using gamelog::application::services::ArtworkType;
using gamelog::application::services::GameArtworkService;
using gamelog::core::AppPaths;
using gamelog::core::domain::Game;
using gamelog::tests::fixtures::FakeNetworkAccessManager;
using gamelog::tests::fixtures::FakeResponse;

namespace
{
    /**
     * Encodes a real, decodable image so validation exercises QImage rather
     * than a hand-written byte blob. Only this file needs image payloads, so the
     * helper stays local instead of becoming a shared fixture.
     */
    QByteArray encodedImage(const char* format)
    {
        QImage image{8, 8, QImage::Format_RGB32};
        image.fill(Qt::red);

        QByteArray bytes;
        QBuffer buffer{&bytes};

        if(!buffer.open(QIODevice::WriteOnly)) { return {}; }
        if(!image.save(&buffer, format)) { return {}; }

        buffer.close();
        return bytes;
    }

    bool writeFile(const QString& filePath, const QByteArray& contents)
    {
        QFile file{filePath};
        if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { return false; }

        const bool written = file.write(contents) == contents.size();
        file.close();
        return written;
    }

    Game makeGame(const int id, const std::optional<int> steamAppId = std::nullopt)
    {
        Game game;
        game.id = id;
        game.title = QStringLiteral("Game %1").arg(id);
        game.steamAppId = steamAppId;
        return game;
    }

    constexpr int kSteamAppId = 620;
    constexpr auto kCoverFragment = "library_600x900.jpg";
    constexpr auto kHeaderFragment = "header.jpg";
    constexpr auto kLogoFragment = "logo.png";
} // namespace

namespace
{
    class GameArtworkServiceTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        void cleanup();

        static void artworkTypeToString_mapsEveryEnumValue();

        void makeGameArtworkDirectory_createsDirectory();

        static void makeGameArtworkDirectory_rejectsNonPositiveIds();

        void getGameArtwork_returnsTrueAndSignalsForValidLocalCover();

        void getGameArtwork_rejectsNonPositiveGameIds();

        void getGameArtwork_queuesNothingWithoutASteamAppId();

        void getGameArtwork_rejectsEmptyCoverFile();

        void getGameArtwork_rejectsDirectoryWithoutCover();

        void getGameArtwork_returnsFalseWhenOnlyQueueingDownloads();

        void getGameArtwork_retriesDownloadOnLaterCalls();

        void getGameArtwork_writesAndSignalsValidatedDownloads();

        void getGameArtwork_headerAndLogoSuccessEmitsNoCoverAvailability();

        void getGameArtwork_rejectsPayloadThatDoesNotDecodeAsTheLabelledFormat();

        void getGameArtwork_signalsUnavailableForNetworkFailures();

    private:
        [[nodiscard]] static QString coverPath(int gameId);

        [[nodiscard]] static QString artworkFilePath(int gameId, const QString& fileName);

        void registerValidResponses() const;

        std::unique_ptr<FakeNetworkAccessManager> networkAccessManager_;
        std::unique_ptr<GameArtworkService> service_;
        std::unique_ptr<QSignalSpy> availableSpy_;
        std::unique_ptr<QSignalSpy> unavailableSpy_;
    };
} // namespace

QString GameArtworkServiceTest::coverPath(const int gameId)
{
    return artworkFilePath(gameId, QStringLiteral("cover.jpg"));
}

QString GameArtworkServiceTest::artworkFilePath(const int gameId, const QString& fileName)
{
    return QDir{AppPaths::gameArtworkDirectory(gameId)}.filePath(fileName);
}

void GameArtworkServiceTest::registerValidResponses() const
{
    networkAccessManager_->setResponseForUrlContaining(QString::fromLatin1(kCoverFragment),
                                                       FakeResponse{200, QNetworkReply::NoError, encodedImage("JPG")});
    networkAccessManager_->setResponseForUrlContaining(QString::fromLatin1(kHeaderFragment),
                                                       FakeResponse{200, QNetworkReply::NoError, encodedImage("JPG")});
    networkAccessManager_->setResponseForUrlContaining(QString::fromLatin1(kLogoFragment),
                                                       FakeResponse{200, QNetworkReply::NoError, encodedImage("PNG")});
}

void GameArtworkServiceTest::init()
{
    // These tests assert on logged messages, so the categories must be on
    // regardless of any ambient QT_LOGGING_RULES.
    gamelog::tests::fixtures::enableGameLogLoggingCategories();

    // Redirects AppPaths away from the developer's real data directory.
    QStandardPaths::setTestModeEnabled(true);
    qRegisterMetaType<ArtworkType>();

    QDir{AppPaths::artworkDirectory()}.removeRecursively();

    networkAccessManager_ = std::make_unique<FakeNetworkAccessManager>();
    networkAccessManager_->setDefaultResponse(FakeResponse{404, QNetworkReply::ContentNotFoundError, QByteArray{}});

    service_ = std::make_unique<GameArtworkService>(*networkAccessManager_);

    availableSpy_ = std::make_unique<QSignalSpy>(service_.get(), &GameArtworkService::artworkAvailable);
    unavailableSpy_ = std::make_unique<QSignalSpy>(service_.get(), &GameArtworkService::artworkUnavailable);

    QVERIFY(availableSpy_->isValid());
    QVERIFY(unavailableSpy_->isValid());
}

void GameArtworkServiceTest::cleanup()
{
    unavailableSpy_.reset();
    availableSpy_.reset();
    service_.reset();
    networkAccessManager_.reset();

    QDir{AppPaths::artworkDirectory()}.removeRecursively();
}

void GameArtworkServiceTest::artworkTypeToString_mapsEveryEnumValue()
{
    QCOMPARE(GameArtworkService::artworkTypeToString(ArtworkType::Cover), QStringLiteral("cover"));
    QCOMPARE(GameArtworkService::artworkTypeToString(ArtworkType::Header), QStringLiteral("header"));
    QCOMPARE(GameArtworkService::artworkTypeToString(ArtworkType::Logo), QStringLiteral("logo"));
}

void GameArtworkServiceTest::makeGameArtworkDirectory_createsDirectory()
{
    QVERIFY(!QDir{AppPaths::gameArtworkDirectory(11)}.exists());
    QVERIFY(GameArtworkService::makeGameArtworkDirectory(11));
    QVERIFY(QDir{AppPaths::gameArtworkDirectory(11)}.exists());

    // Creating an existing directory stays successful.
    QVERIFY(GameArtworkService::makeGameArtworkDirectory(11));
}

void GameArtworkServiceTest::makeGameArtworkDirectory_rejectsNonPositiveIds()
{
    QVERIFY(!GameArtworkService::makeGameArtworkDirectory(0));
    QVERIFY(!GameArtworkService::makeGameArtworkDirectory(-1));
}

void GameArtworkServiceTest::getGameArtwork_returnsTrueAndSignalsForValidLocalCover()
{
    const Game game = makeGame(1);
    QVERIFY(GameArtworkService::makeGameArtworkDirectory(game.id));
    QVERIFY(writeFile(coverPath(game.id), encodedImage("JPG")));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*not recorded in the database.*"));
    QVERIFY(service_->getGameArtwork(game));

    QCOMPARE(availableSpy_->count(), 1);
    QCOMPARE(availableSpy_->at(0).at(0).toInt(), game.id);
    QCOMPARE(availableSpy_->at(0).at(1).value<ArtworkType>(), ArtworkType::Cover);
    QCOMPARE(unavailableSpy_->count(), 0);
    QCOMPARE(networkAccessManager_->requestCount(), 0);
}

void GameArtworkServiceTest::getGameArtwork_rejectsNonPositiveGameIds()
{
    QVERIFY(!service_->getGameArtwork(makeGame(0, kSteamAppId)));
    QVERIFY(!service_->getGameArtwork(makeGame(-5, kSteamAppId)));

    QCOMPARE(availableSpy_->count(), 0);
    QCOMPARE(unavailableSpy_->count(), 0);
    QCOMPARE(networkAccessManager_->requestCount(), 0);
}

void GameArtworkServiceTest::getGameArtwork_queuesNothingWithoutASteamAppId()
{
    QVERIFY(!service_->getGameArtwork(makeGame(2)));
    QCOMPARE(networkAccessManager_->requestCount(), 0);

    // A non-positive Steam App ID is also not a usable download identity.
    QVERIFY(!service_->getGameArtwork(makeGame(3, 0)));
    QVERIFY(!service_->getGameArtwork(makeGame(4, -1)));
    QCOMPARE(networkAccessManager_->requestCount(), 0);

    QCOMPARE(unavailableSpy_->count(), 3);
    QCOMPARE(availableSpy_->count(), 0);
}

void GameArtworkServiceTest::getGameArtwork_rejectsEmptyCoverFile()
{
    const Game game = makeGame(5);
    QVERIFY(GameArtworkService::makeGameArtworkDirectory(game.id));
    QVERIFY(writeFile(coverPath(game.id), QByteArray{}));

    QVERIFY(!service_->getGameArtwork(game));
    QCOMPARE(availableSpy_->count(), 0);
    QCOMPARE(unavailableSpy_->count(), 1);
    QCOMPARE(unavailableSpy_->at(0).at(1).value<ArtworkType>(), ArtworkType::Cover);
}

void GameArtworkServiceTest::getGameArtwork_rejectsDirectoryWithoutCover()
{
    const Game game = makeGame(6);
    QVERIFY(GameArtworkService::makeGameArtworkDirectory(game.id));

    QVERIFY(!service_->getGameArtwork(game));
    QCOMPARE(availableSpy_->count(), 0);
    QCOMPARE(unavailableSpy_->count(), 1);
}

void GameArtworkServiceTest::getGameArtwork_returnsFalseWhenOnlyQueueingDownloads()
{
    registerValidResponses();

    const Game game = makeGame(7, kSteamAppId);

    // Downloads are queued but no local cover exists yet, so the call is false.
    QVERIFY(!service_->getGameArtwork(game));
    QCOMPARE(networkAccessManager_->requestCount(), 3);
    QVERIFY(!QFile::exists(coverPath(game.id)));
}

void GameArtworkServiceTest::getGameArtwork_retriesDownloadOnLaterCalls()
{
    const Game game = makeGame(8, kSteamAppId);

    QVERIFY(!service_->getGameArtwork(game));
    QCOMPARE(networkAccessManager_->requestCount(), 3);

    // A missing or invalid cover is retried rather than being remembered as failed.
    QVERIFY(!service_->getGameArtwork(game));
    QCOMPARE(networkAccessManager_->requestCount(), 6);
}

void GameArtworkServiceTest::getGameArtwork_writesAndSignalsValidatedDownloads()
{
    registerValidResponses();

    const Game game = makeGame(9, kSteamAppId);
    QVERIFY(!service_->getGameArtwork(game));

    QTRY_COMPARE_WITH_TIMEOUT(availableSpy_->count(), 3, 5000);

    QVERIFY(QFile::exists(coverPath(game.id)));
    QVERIFY(QFile::exists(artworkFilePath(game.id, QStringLiteral("header.jpg"))));
    QVERIFY(QFile::exists(artworkFilePath(game.id, QStringLiteral("logo.png"))));

    QList<ArtworkType> signalledTypes;
    for(const QList<QVariant>& arguments : *availableSpy_)
    {
        QCOMPARE(arguments.at(0).toInt(), game.id);
        signalledTypes.push_back(arguments.at(1).value<ArtworkType>());
    }

    QVERIFY(signalledTypes.contains(ArtworkType::Cover));
    QVERIFY(signalledTypes.contains(ArtworkType::Header));
    QVERIFY(signalledTypes.contains(ArtworkType::Logo));

    // The cover is now locally valid, so a later call succeeds without downloads.
    networkAccessManager_->clearRecordedRequests();
    QVERIFY(service_->getGameArtwork(game));
    QCOMPARE(networkAccessManager_->requestCount(), 0);
}

void GameArtworkServiceTest::getGameArtwork_headerAndLogoSuccessEmitsNoCoverAvailability()
{
    networkAccessManager_->setResponseForUrlContaining(QString::fromLatin1(kCoverFragment),
                                                       FakeResponse{
                                                           404, QNetworkReply::ContentNotFoundError, QByteArray{}
                                                       });
    networkAccessManager_->setResponseForUrlContaining(QString::fromLatin1(kHeaderFragment),
                                                       FakeResponse{200, QNetworkReply::NoError, encodedImage("JPG")});
    networkAccessManager_->setResponseForUrlContaining(QString::fromLatin1(kLogoFragment),
                                                       FakeResponse{200, QNetworkReply::NoError, encodedImage("PNG")});

    const Game game = makeGame(10, kSteamAppId);
    QVERIFY(!service_->getGameArtwork(game));

    QTRY_COMPARE_WITH_TIMEOUT(availableSpy_->count(), 2, 5000);

    for(const QList<QVariant>& arguments : *availableSpy_)
    {
        QVERIFY(arguments.at(1).value<ArtworkType>() != ArtworkType::Cover);
    }

    QVERIFY(!QFile::exists(coverPath(game.id)));
    QVERIFY(QFile::exists(artworkFilePath(game.id, QStringLiteral("header.jpg"))));
    QVERIFY(QFile::exists(artworkFilePath(game.id, QStringLiteral("logo.png"))));

    // Only cover availability may establish local completeness.
    QVERIFY(!service_->getGameArtwork(game));
}

void GameArtworkServiceTest::getGameArtwork_rejectsPayloadThatDoesNotDecodeAsTheLabelledFormat()
{
    // A JPEG endpoint answering with PNG bytes, and undecodable junk, must both
    // fail validation and leave nothing on disk.
    networkAccessManager_->setResponseForUrlContaining(QString::fromLatin1(kCoverFragment),
                                                       FakeResponse{200, QNetworkReply::NoError, encodedImage("PNG")});
    networkAccessManager_->setResponseForUrlContaining(QString::fromLatin1(kHeaderFragment),
                                                       FakeResponse{
                                                           200, QNetworkReply::NoError,
                                                           QByteArrayLiteral("definitely not an image")
                                                       });
    networkAccessManager_->setResponseForUrlContaining(QString::fromLatin1(kLogoFragment),
                                                       FakeResponse{200, QNetworkReply::NoError, QByteArray{}});

    const Game game = makeGame(12, kSteamAppId);
    QVERIFY(!service_->getGameArtwork(game));

    QTRY_COMPARE_WITH_TIMEOUT(unavailableSpy_->count(), 4, 5000);

    QCOMPARE(availableSpy_->count(), 0);
    QVERIFY(!QFile::exists(coverPath(game.id)));
    QVERIFY(!QFile::exists(artworkFilePath(game.id, QStringLiteral("header.jpg"))));
    QVERIFY(!QFile::exists(artworkFilePath(game.id, QStringLiteral("logo.png"))));
}

void GameArtworkServiceTest::getGameArtwork_signalsUnavailableForNetworkFailures()
{
    // The default response registered in init() fails every URL.
    const Game game = makeGame(13, kSteamAppId);
    QVERIFY(!service_->getGameArtwork(game));

    QTRY_COMPARE_WITH_TIMEOUT(unavailableSpy_->count(), 4, 5000);

    QCOMPARE(availableSpy_->count(), 0);

    QList<ArtworkType> failedTypes;
    for(qsizetype index = 1; index < unavailableSpy_->count(); ++index)
    {
        const QList<QVariant> arguments = unavailableSpy_->at(index);
        QCOMPARE(arguments.at(0).toInt(), game.id);
        failedTypes.push_back(arguments.at(1).value<ArtworkType>());
    }

    QVERIFY(failedTypes.contains(ArtworkType::Cover));
    QVERIFY(failedTypes.contains(ArtworkType::Header));
    QVERIFY(failedTypes.contains(ArtworkType::Logo));
}

QTEST_GUILESS_MAIN(GameArtworkServiceTest)

#include "GameArtworkServiceTest.moc"
