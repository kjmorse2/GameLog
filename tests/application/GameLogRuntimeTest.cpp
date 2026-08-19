#include <QtTest/QtTest>

#include "application/GameLogRuntime.h"
#include "application/services/web/GameArtworkService.h"
#include "database/DatabaseManager.h"
#include "fixtures/FakeProcessSource.h"
#include "fixtures/TestDatabaseFixture.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

using gamelog::application::GameLogRuntime;
using gamelog::application::services::ArtworkType;
using gamelog::core::database::DatabaseManager;
using gamelog::core::process::ProcessInfo;
using gamelog::core::process::ProcessSource;
using gamelog::tests::fixtures::FakeProcessSource;
using std::chrono::seconds;

namespace
{
    ProcessInfo makeProcess(const qint64 pid, const QString& executablePath)
    {
        ProcessInfo process;
        process.pid = pid;
        process.executablePath = executablePath;
        process.executableName = executablePath.section(QLatin1Char('/'), -1);
        return process;
    }
} // namespace

namespace
{
    class GameLogRuntimeTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        void cleanup();

        void construction_exposesEveryServiceForAReadyDatabase();

        void construction_returnsNullServicesWhenDatabaseInitializationFails();

        void start_succeedsAndSyncsServices();

        void start_failsWhenTheProcessSourceFactoryReturnsNull();

        void start_failsWhileAlreadyRunning();

        void update_pollsTheProcessSource();

        void update_isANoOpBeforeStartAndAfterStop();

        void update_isANoOpForNonPositiveElapsed();

        void stop_completesAndLeavesServicesUsable();

        void startStopStart_recreatesProcessSourceAndRestoresState();

        void artworkSignals_persistOnlyCoverAvailability();

    private:
        [[nodiscard]] int seedGame(const QString& title, const QString& executablePath) const;

        [[nodiscard]] bool seedActiveSession(int gameId) const;

        [[nodiscard]] GameLogRuntime::ProcessSourceFactory makeFactory();

        [[nodiscard]] std::unique_ptr<GameLogRuntime> makeRuntime();

        QString databasePath_;
        QString seedConnectionName_;
        std::vector<ProcessInfo> processes_;
        int processSourceFactoryCalls_{0};
        FakeProcessSource* lastProcessSource_{nullptr};
    };
} // namespace

void GameLogRuntimeTest::init()
{
    databasePath_ =
        gamelog::tests::fixtures::createFreshTestDatabasePath(QStringLiteral("gamelog-runtime-%1").
                                                              arg(QString::fromLatin1(QTest::currentTestFunction())));

    seedConnectionName_ = gamelog::tests::fixtures::createUniqueConnectionName("gamelog-runtime-seed");
    processes_.clear();
    processSourceFactoryCalls_ = 0;
    lastProcessSource_ = nullptr;

    // The runtime owns a fixed connection name, so seeding happens through a
    // separate manager that is fully released before the runtime is built.
    DatabaseManager seedManager{databasePath_, seedConnectionName_};
    QVERIFY(seedManager.initialize());
}

void GameLogRuntimeTest::cleanup() { gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath_); }

void GameLogRuntimeTest::artworkSignals_persistOnlyCoverAvailability()
{
    const int gameId = seedGame(QStringLiteral("Artwork Game"), QStringLiteral("/games/artwork"));
    QVERIFY(gameId > 0);

    const std::unique_ptr<GameLogRuntime> runtime = makeRuntime();

    auto* artworkService = runtime->getArtworkService();
    auto* gameService = runtime->getGameService();
    QVERIFY(artworkService != nullptr);
    QVERIFY(gameService != nullptr);
    QVERIFY(!gameService->findById(gameId)->hasArtwork);

    emit
    artworkService->artworkAvailable(gameId, ArtworkType::Cover);
    QVERIFY(gameService->findById(gameId)->hasArtwork);

    // Header and logo results describe partial success and must not change the
    // persisted cover-availability flag in either direction.
    emit
    artworkService->artworkUnavailable(gameId, ArtworkType::Header);
    emit
    artworkService->artworkUnavailable(gameId, ArtworkType::Logo);
    QVERIFY(gameService->findById(gameId)->hasArtwork);

    emit
    artworkService->artworkUnavailable(gameId, ArtworkType::Cover);
    QVERIFY(!gameService->findById(gameId)->hasArtwork);

    emit
    artworkService->artworkAvailable(gameId, ArtworkType::Header);
    emit
    artworkService->artworkAvailable(gameId, ArtworkType::Logo);
    QVERIFY(!gameService->findById(gameId)->hasArtwork);
}

int GameLogRuntimeTest::seedGame(const QString& title, const QString& executablePath) const
{
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("gamelog-runtime-seed-game");
    int gameId = 0;

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath_);

        if(database.open())
        {
            QSqlQuery query{database};
            query.prepare(QStringLiteral("INSERT INTO games (title, executable_path, executable_name, "
                                         "tracking_enabled) VALUES (:title, :path, :name, 1)"));
            query.bindValue(QStringLiteral(":title"), title);
            query.bindValue(QStringLiteral(":path"), executablePath);
            query.bindValue(QStringLiteral(":name"), executablePath.section(QLatin1Char('/'), -1));

            if(query.exec()) { gameId = query.lastInsertId().toInt(); }

            database.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
    return gameId;
}

bool GameLogRuntimeTest::seedActiveSession(const int gameId) const
{
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("gamelog-runtime-seed-session");
    bool inserted = false;

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath_);

        if(database.open())
        {
            const QString startedAt = QDateTime::currentDateTimeUtc().addSecs(-600).toString(Qt::ISODateWithMs);

            QSqlQuery sessionQuery{database};
            sessionQuery.prepare(QStringLiteral("INSERT INTO sessions (game_id, start_timestamp_utc, "
                                                "tracked_duration_seconds, source, status) VALUES (:game_id, :start, "
                                                "0, 'automatic', 'active')"));
            sessionQuery.bindValue(QStringLiteral(":game_id"), gameId);
            sessionQuery.bindValue(QStringLiteral(":start"), startedAt);

            if(sessionQuery.exec())
            {
                QSqlQuery documentQuery{database};
                documentQuery.prepare(QStringLiteral("INSERT INTO session_documents (session_id, content, "
                                                     "last_saved_timestamp_utc) VALUES (:session_id, '', :saved_at)"));
                documentQuery.bindValue(QStringLiteral(":session_id"), sessionQuery.lastInsertId().toInt());
                documentQuery.bindValue(QStringLiteral(":saved_at"), startedAt);
                inserted = documentQuery.exec();
            }

            database.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
    return inserted;
}

GameLogRuntime::ProcessSourceFactory GameLogRuntimeTest::makeFactory()
{
    return [this]() -> std::unique_ptr<ProcessSource>
    {
        ++processSourceFactoryCalls_;

        auto source = std::make_unique<FakeProcessSource>(processes_);
        lastProcessSource_ = source.get();
        return source;
    };
}

std::unique_ptr<GameLogRuntime> GameLogRuntimeTest::makeRuntime()
{
    return std::make_unique<GameLogRuntime>(databasePath_,
                                            makeFactory(),
                                            [](qint64) { return std::optional<std::uint32_t>{}; });
}

void GameLogRuntimeTest::construction_exposesEveryServiceForAReadyDatabase()
{
    const std::unique_ptr<GameLogRuntime> runtime = makeRuntime();

    QVERIFY(runtime->getGameService() != nullptr);
    QVERIFY(runtime->getSessionService() != nullptr);
    QVERIFY(runtime->getArtworkService() != nullptr);
    QVERIFY(runtime->getCredentialService() != nullptr);
}

void GameLogRuntimeTest::construction_returnsNullServicesWhenDatabaseInitializationFails()
{
    GameLogRuntime runtime{QStringLiteral("   "), makeFactory(), [](qint64) { return std::optional<std::uint32_t>{}; }};

    QVERIFY(runtime.getGameService() == nullptr);
    QVERIFY(runtime.getSessionService() == nullptr);
    QVERIFY(runtime.getArtworkService() == nullptr);
    QVERIFY(runtime.getCredentialService() == nullptr);

    QVERIFY(!runtime.start());
    QCOMPARE(processSourceFactoryCalls_, 0);
}

void GameLogRuntimeTest::start_succeedsAndSyncsServices()
{
    const int gameId = seedGame(QStringLiteral("Tracked Game"), QStringLiteral("/games/tracked"));
    QVERIFY(gameId > 0);

    const std::unique_ptr<GameLogRuntime> runtime = makeRuntime();

    QVERIFY(runtime->start());
    QCOMPARE(processSourceFactoryCalls_, 1);

    // start() rebuilds the process-matching index from persistence.
    QCOMPARE(runtime->getGameService()->trackedPathGames().size(), 1);

    runtime->stop();
}

void GameLogRuntimeTest::start_failsWhenTheProcessSourceFactoryReturnsNull()
{
    GameLogRuntime runtime{
        databasePath_, [] { return std::unique_ptr<ProcessSource>{}; },
        [](qint64) { return std::optional<std::uint32_t>{}; }
    };

    QVERIFY(!runtime.start());

    // A failed start leaves the runtime stopped rather than half-running.
    runtime.update(seconds{1});
}

void GameLogRuntimeTest::start_failsWhileAlreadyRunning()
{
    const std::unique_ptr<GameLogRuntime> runtime = makeRuntime();

    QVERIFY(runtime->start());
    QVERIFY(!runtime->start());
    QCOMPARE(processSourceFactoryCalls_, 1);

    runtime->stop();
}

void GameLogRuntimeTest::update_pollsTheProcessSource()
{
    processes_ = {makeProcess(100, QStringLiteral("/games/tracked"))};

    const std::unique_ptr<GameLogRuntime> runtime = makeRuntime();
    QVERIFY(runtime->start());
    QVERIFY(lastProcessSource_ != nullptr);

    runtime->update(seconds{1});
    QCOMPARE(lastProcessSource_->listProcessesCallCount(), 1);

    runtime->update(seconds{1});
    QCOMPARE(lastProcessSource_->listProcessesCallCount(), 2);

    runtime->stop();
}

void GameLogRuntimeTest::update_isANoOpBeforeStartAndAfterStop()
{
    const std::unique_ptr<GameLogRuntime> runtime = makeRuntime();

    runtime->update(seconds{1});
    QCOMPARE(processSourceFactoryCalls_, 0);

    QVERIFY(runtime->start());
    FakeProcessSource* source = lastProcessSource_;
    QVERIFY(source != nullptr);

    runtime->update(seconds{1});
    QCOMPARE(source->listProcessesCallCount(), 1);

    runtime->stop();

    // The process source is released on stop, so no further polling can occur.
    runtime->update(seconds{1});
    QCOMPARE(processSourceFactoryCalls_, 1);
}

void GameLogRuntimeTest::update_isANoOpForNonPositiveElapsed()
{
    const std::unique_ptr<GameLogRuntime> runtime = makeRuntime();
    QVERIFY(runtime->start());
    QVERIFY(lastProcessSource_ != nullptr);

    runtime->update(seconds{0});
    runtime->update(seconds{-5});

    QCOMPARE(lastProcessSource_->listProcessesCallCount(), 0);

    runtime->stop();
}

void GameLogRuntimeTest::stop_completesAndLeavesServicesUsable()
{
    const std::unique_ptr<GameLogRuntime> runtime = makeRuntime();
    QVERIFY(runtime->start());

    runtime->stop();

    // stop() is idempotent and does not tear down the owned services.
    runtime->stop();
    QVERIFY(runtime->getGameService() != nullptr);
    QVERIFY(runtime->getSessionService() != nullptr);
}

void GameLogRuntimeTest::startStopStart_recreatesProcessSourceAndRestoresState()
{
    const int gameId = seedGame(QStringLiteral("Restored Game"), QStringLiteral("/games/restored"));
    QVERIFY(gameId > 0);
    QVERIFY(seedActiveSession(gameId));

    const std::unique_ptr<GameLogRuntime> runtime = makeRuntime();

    QVERIFY(runtime->start());
    QCOMPARE(processSourceFactoryCalls_, 1);

    const auto firstRestored = runtime->getSessionService()->findActiveSession();
    QVERIFY(firstRestored.has_value());
    QCOMPARE(firstRestored->gameId, gameId);

    runtime->stop();

    QVERIFY(runtime->start());
    QCOMPARE(processSourceFactoryCalls_, 2);

    const auto secondRestored = runtime->getSessionService()->findActiveSession();
    QVERIFY(secondRestored.has_value());
    QCOMPARE(secondRestored->id, firstRestored->id);

    runtime->stop();
}

QTEST_GUILESS_MAIN(GameLogRuntimeTest)

#include "GameLogRuntimeTest.moc"
