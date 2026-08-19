#include <QSqlQuery>
#include <QTimeZone>
#include <memory>
#include <QtTest/QtTest>
#include <application/services/local/CredentialService.h>
#include <application/services/web/SteamApiService.h>

#include <chrono>
#include <optional>
#include <vector>

#include <QSignalSpy>

#include "domain/Game.h"
#include "domain/query/SessionQuery.h"
#include "process/ProcessInfo.h"

#include "application/services/local/GameService.h"
#include "application/services/local/SessionService.h"
#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "fixtures/TestDatabaseFixture.h"

using gamelog::core::database::DatabaseManager;
using gamelog::core::database::SessionRepository;
using gamelog::core::database::GameRepository;
using gamelog::core::domain::Session;
using gamelog::core::domain::SessionSource;
using gamelog::application::services::SessionService;
using gamelog::application::services::GameService;
using gamelog::core::domain::SessionStatus;
using gamelog::application::services::CredentialService;
using gamelog::application::services::SteamApiService;
using gamelog::core::domain::Game;
using gamelog::core::domain::query::SessionQuery;
using gamelog::core::domain::query::SessionSortField;
using gamelog::core::domain::query::SortDirection;
using gamelog::core::process::ProcessInfo;
using std::chrono::seconds;

namespace
{
    /**
     * Keeps every test keychain-free. SessionService only needs GameService,
     * which in turn needs a SteamApiService, which needs a CredentialService.
     */
    class KeychainFreeCredentialService : public CredentialService
    {
    protected:
        QKeychain::WritePasswordJob* createWritePasswordJob() override { return nullptr; }

        QKeychain::ReadPasswordJob* createReadPasswordJob() override { return nullptr; }

        QKeychain::DeletePasswordJob* createDeletePasswordJob() override { return nullptr; }
    };

    ProcessInfo makeProcess(const qint64 pid,
                            const QString& executablePath,
                            const std::optional<std::uint32_t> steamAppId = std::nullopt)
    {
        ProcessInfo process;
        process.pid = pid;
        process.executablePath = executablePath;
        process.executableName = executablePath.section(QLatin1Char('/'), -1);
        process.steamAppId = steamAppId;
        return process;
    }
} // namespace

namespace
{
    class SessionServiceTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        void cleanup();

        void findActiveSession_returnsNulloptWhenNoActiveSessionExists();

        void findActiveSession_returnsInsertedActiveSession();

        void listSessionsForGame_returnsNewestFirstAndFiltersByGame();

        void listSessionsForGame_returnsEmptyWhenNoRowsExist();

        void addSession_persistsSessionAndAssignsId();

        void addSession_failsForUnknownGameId();

        void updateSession_persistsModifiedFields();

        void updateSession_returnsFalseForMissingRow();

        void removeSession_deletesExistingRow();

        void removeSession_returnsFalseForMissingRow();

        void search_returnsSessionsMatchingAPopulatedQuery();

        void getSessionsInTimeRange_returnsSessionsStartingInsideTheWindow();

        void getSessionsInTimeRange_usesAHalfOpenInterval();

        void startAutomaticSession_createsAnActiveSessionForATrackedGame();

        void startAutomaticSession_rejectsMissingAndUntrackedGames();

        void startAutomaticSession_rejectsASecondActiveSession();

        void endActiveSession_completesTheActiveSession();

        void endActiveSession_returnsNulloptWithoutAnActiveSession();

        void endActiveSession_replacesTrackedDurationWithWallClock();

        void endActiveSession_failsWithoutTouchingPersistenceForAnInvalidClock();

        void endActiveSession_failsWithoutTouchingPersistenceForAnEarlierClock();

        void restoreActiveSession_succeedsWithNoActiveRows();

        void restoreActiveSession_restoresTheSingleActiveRow();

        void restoreActiveSession_interruptsAnOrphanedActiveRow();

        void updateAutomaticTracking_startsASessionAfterTheStartGracePeriod();

        void updateAutomaticTracking_endsASessionAfterTheEndGracePeriod();

        void updateAutomaticTracking_ignoresNonPositiveElapsed();

        void updateAutomaticTracking_retainsAStillDetectedPendingGame();

        void updateAutomaticTracking_prefersSteamIdentityOverPathIdentity();

        void updateAutomaticTracking_prefersTheLowerGameIdOnATie();

        void updateAutomaticTracking_ignoresUntrackedGames();

        void resetAutomaticTracking_clearsPendingStartState();

        void addSession_rejectsASecondActiveSession();

        void updateSession_rejectsPromotingASecondSessionToActive();

        void addSession_emitsSessionStartedForAnActiveSession();

        void addSession_emitsNoLifecycleSignalForAnInactiveSession();

        void updateSession_emitsSessionStoppedWhenLeavingActive();

        void updateSession_emitsNoSignalForActiveToActiveEdits();

        void updateSession_emitsNoSignalForInactiveToInactiveEdits();

        void updateSession_emitsSessionStartedWhenBecomingActive();

        void removeSession_rejectsAnActiveSession();

        void removeSession_emitsNoLifecycleSignalForInactiveRows();

        void sessionStopped_deliversAModifiableSessionCopy();

        static void session_isADeclaredQtMetatype();

    private:
        int addSessionGame(const QString& title) const;

        int addTrackedGame(const QString& title,
                           const QString& executablePath,
                           std::optional<int> steamAppId = std::nullopt,
                           bool trackingEnabled = true) const;

        [[nodiscard]] int activeRowCount() const;

        [[nodiscard]] std::optional<Session> reloadSession(int sessionId) const;

        static Session makeSession(int gameId, const QDateTime& startUtc, SessionStatus status);

        static QDateTime utcDateTime(int year, int month, int day, int hour, int minute, int second);

        QString databasePath_;
        std::unique_ptr<DatabaseManager> manager_;
        std::unique_ptr<GameRepository> game_repo_;
        std::unique_ptr<KeychainFreeCredentialService> credential_service_;
        std::unique_ptr<SteamApiService> steam_api_service_;
        std::unique_ptr<GameService> game_service_;
        std::unique_ptr<SessionRepository> service_repo_;
        std::unique_ptr<SessionService> service_;

        /**
         * @brief Value returned by the injected clock, so lifecycle timestamps
         *        are deterministic instead of depending on wall-clock time.
         */
        QDateTime currentTime_;
    };
}

void SessionServiceTest::init()
{
    databasePath_ =
        gamelog::tests::fixtures::createFreshTestDatabasePath(QString{"session-repository-%1"}.
                                                              arg(QTest::currentTestFunction()));
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("session-repository");

    manager_ = std::make_unique<DatabaseManager>(databasePath_, connectionName);
    QVERIFY(manager_->initialize());

    game_repo_ = std::make_unique<GameRepository>(manager_->database());

    // GameService stores a SteamApiService reference, so both collaborators must
    // outlive it rather than being init()-scoped temporaries.
    credential_service_ = std::make_unique<KeychainFreeCredentialService>();
    steam_api_service_ = std::make_unique<SteamApiService>(*credential_service_);
    game_service_ = std::make_unique<GameService>(*game_repo_, *steam_api_service_);

    service_repo_ = std::make_unique<SessionRepository>(manager_->database());

    // The clock-injecting constructor keeps grace periods and wall-clock
    // duration replacement deterministic.
    currentTime_ = utcDateTime(2026, 6, 1, 12, 0, 0);
    service_ = std::make_unique<SessionService>(*service_repo_, *game_service_, [this] { return currentTime_; });
}

void SessionServiceTest::cleanup()
{
    service_.reset();
    service_repo_.reset();
    game_service_.reset();
    steam_api_service_.reset();
    credential_service_.reset();
    game_repo_.reset();
    manager_.reset();
    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath_);
}

void SessionServiceTest::findActiveSession_returnsNulloptWhenNoActiveSessionExists()
{
    const auto active = service_->findActiveSession();
    QVERIFY(!active.has_value());
}

void SessionServiceTest::findActiveSession_returnsInsertedActiveSession()
{
    const int gameId = addSessionGame("Elden Ring");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 7, 1, 12, 34, 56), SessionStatus::Active);
    QVERIFY(service_->addSession(session));

    const auto active = service_->findActiveSession();
    QVERIFY(active.has_value());
    QCOMPARE(active->id, session.id);
    QCOMPARE(active->gameId, gameId);
    QCOMPARE(active->startTimestamp.toUTC(), session.startTimestamp.toUTC());
    QVERIFY(!active->endTimestamp.has_value());
    QCOMPARE(active->trackedDuration, std::chrono::seconds{120});
    QCOMPARE(active->source, SessionSource::Automatic);
    QCOMPARE(active->status, SessionStatus::Active);
}

void SessionServiceTest::listSessionsForGame_returnsNewestFirstAndFiltersByGame()
{
    const int gameOneId = addSessionGame("Game One");
    const int gameTwoId = addSessionGame("Game Two");
    QVERIFY(gameOneId > 0);
    QVERIFY(gameTwoId > 0);

    Session oldest = makeSession(gameOneId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Completed);
    oldest.endTimestamp = utcDateTime(2026, 1, 1, 9, 0, 0);

    Session newest = makeSession(gameOneId, utcDateTime(2026, 1, 2, 8, 0, 0), SessionStatus::Interrupted);
    newest.endTimestamp = utcDateTime(2026, 1, 2, 8, 30, 0);

    Session otherGame = makeSession(gameTwoId, utcDateTime(2026, 1, 3, 8, 0, 0), SessionStatus::Completed);

    QVERIFY(service_->addSession(oldest));
    QVERIFY(service_->addSession(newest));
    QVERIFY(service_->addSession(otherGame));

    const auto sessions = service_->listSessionsForGame(gameOneId);
    QCOMPARE(sessions.size(), 2);
    QCOMPARE(sessions[0].id, newest.id);
    QCOMPARE(sessions[1].id, oldest.id);
    QCOMPARE(sessions[0].gameId, gameOneId);
    QCOMPARE(sessions[1].gameId, gameOneId);
}

void SessionServiceTest::listSessionsForGame_returnsEmptyWhenNoRowsExist()
{
    const int gameId = addSessionGame("No Sessions");
    QVERIFY(gameId > 0);

    const auto sessions = service_->listSessionsForGame(gameId);
    QVERIFY(sessions.empty());
}

void SessionServiceTest::addSession_persistsSessionAndAssignsId()
{
    const int gameId = addSessionGame("Hades");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 5, 5, 18, 10, 0), SessionStatus::Completed);
    session.source = SessionSource::Manual;
    session.endTimestamp = utcDateTime(2026, 5, 5, 18, 40, 0);
    session.trackedDuration = std::chrono::seconds{1800};

    QVERIFY(service_->addSession(session));
    QVERIFY(session.id > 0);

    const auto sessions = service_->listSessionsForGame(gameId);
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions[0].id, session.id);
    QCOMPARE(sessions[0].source, SessionSource::Manual);
    QCOMPARE(sessions[0].status, SessionStatus::Completed);
    QVERIFY(sessions[0].endTimestamp.has_value());
}

void SessionServiceTest::addSession_failsForUnknownGameId()
{
    Session session = makeSession(999999, utcDateTime(2026, 6, 1, 10, 0, 0), SessionStatus::Active);
    QVERIFY(!service_->addSession(session));
    QCOMPARE(session.id, 0);
}

void SessionServiceTest::updateSession_persistsModifiedFields()
{
    const int gameId = addSessionGame("Terraria");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 3, 1, 11, 0, 0), SessionStatus::Completed);
    QVERIFY(service_->addSession(session));

    session.source = SessionSource::Manual;
    session.endTimestamp = utcDateTime(2026, 3, 1, 11, 45, 0);
    session.trackedDuration = std::chrono::seconds{2700};

    QVERIFY(service_->updateSession(session));

    const auto sessions = service_->listSessionsForGame(gameId);
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions[0].id, session.id);
    QCOMPARE(sessions[0].source, SessionSource::Manual);
    QCOMPARE(sessions[0].status, SessionStatus::Completed);
    QVERIFY(sessions[0].endTimestamp.has_value());
    QCOMPARE(sessions[0].endTimestamp->toUTC(), session.endTimestamp->toUTC());
    QCOMPARE(sessions[0].trackedDuration, std::chrono::seconds{2700});
}

void SessionServiceTest::updateSession_returnsFalseForMissingRow()
{
    const int gameId = addSessionGame("Missing Session");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 4, 2, 9, 15, 0), SessionStatus::Active);
    session.id = 999999;

    QVERIFY(!service_->updateSession(session));
}

void SessionServiceTest::removeSession_deletesExistingRow()
{
    const int gameId = addSessionGame("Delete Session");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 2, 2, 20, 0, 0), SessionStatus::Completed);
    QVERIFY(service_->addSession(session));

    QVERIFY(service_->removeSession(session.id));
    const auto sessions = service_->listSessionsForGame(gameId);
    QVERIFY(sessions.empty());
}

void SessionServiceTest::removeSession_returnsFalseForMissingRow() { QVERIFY(!service_->removeSession(999999)); }

void SessionServiceTest::search_returnsSessionsMatchingAPopulatedQuery()
{
    const int gameOneId = addSessionGame("Query Game One");
    const int gameTwoId = addSessionGame("Query Game Two");

    Session manualOne = makeSession(gameOneId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Completed);
    manualOne.source = SessionSource::Manual;
    manualOne.trackedDuration = seconds{600};

    Session automaticOne = makeSession(gameOneId, utcDateTime(2026, 1, 2, 8, 0, 0), SessionStatus::Interrupted);
    automaticOne.trackedDuration = seconds{60};

    Session otherGame = makeSession(gameTwoId, utcDateTime(2026, 1, 3, 8, 0, 0), SessionStatus::Completed);
    otherGame.source = SessionSource::Manual;

    QVERIFY(service_->addSession(manualOne));
    QVERIFY(service_->addSession(automaticOne));
    QVERIFY(service_->addSession(otherGame));

    SessionQuery query;
    query.gameIds = {gameOneId};
    query.sources = {SessionSource::Manual};
    query.statuses = {SessionStatus::Completed};
    query.minimumTrackedDuration = seconds{300};
    query.hasEndTimestamp = true;
    query.sortBy = SessionSortField::Id;
    query.sortDirection = SortDirection::Ascending;
    query.limit = std::size_t{5};

    const auto sessions = service_->search(query);
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions[0].id, manualOne.id);
}

void SessionServiceTest::getSessionsInTimeRange_returnsSessionsStartingInsideTheWindow()
{
    const int gameId = addSessionGame("Windowed Game");

    Session before = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Completed);
    Session inside = makeSession(gameId, utcDateTime(2026, 1, 5, 8, 0, 0), SessionStatus::Completed);
    Session after = makeSession(gameId, utcDateTime(2026, 1, 9, 8, 0, 0), SessionStatus::Completed);

    QVERIFY(service_->addSession(before));
    QVERIFY(service_->addSession(inside));
    QVERIFY(service_->addSession(after));

    const auto sessions = service_->getSessionsInTimeRange(utcDateTime(2026, 1, 3, 0, 0, 0),
                                                           utcDateTime(2026, 1, 7, 0, 0, 0));
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions[0].id, inside.id);

    QVERIFY(service_->getSessionsInTimeRange(utcDateTime(2026, 2, 1, 0, 0, 0), utcDateTime(2026, 3, 1, 0, 0, 0)).
                      empty());
}

void SessionServiceTest::getSessionsInTimeRange_usesAHalfOpenInterval()
{
    const int gameId = addSessionGame("Boundary Game");
    const QDateTime start = utcDateTime(2026, 4, 1, 12, 0, 0);

    Session session = makeSession(gameId, start, SessionStatus::Completed);
    QVERIFY(service_->addSession(session));

    // The lower bound is inclusive.
    QCOMPARE(service_->getSessionsInTimeRange(start, start.addSecs(3600)).size(), 1);

    // The upper bound is exclusive at exactly the start instant.
    QVERIFY(service_->getSessionsInTimeRange(start.addSecs(-3600), start).empty());
    QCOMPARE(service_->getSessionsInTimeRange(start.addSecs(-3600), start.addMSecs(1)).size(), 1);

    // One millisecond after the start excludes it again.
    QVERIFY(service_->getSessionsInTimeRange(start.addMSecs(1), start.addSecs(3600)).empty());
}

void SessionServiceTest::startAutomaticSession_createsAnActiveSessionForATrackedGame()
{
    const int gameId = addTrackedGame("Auto Game", "/games/auto");

    const QSignalSpy startedSpy{service_.get(), &SessionService::sessionStarted};
    QVERIFY(startedSpy.isValid());

    const auto session = service_->startAutomaticSession(gameId);
    QVERIFY(session.has_value());
    QVERIFY(session->id > 0);
    QCOMPARE(session->gameId, gameId);
    QCOMPARE(session->status, SessionStatus::Active);
    QCOMPARE(session->source, SessionSource::Automatic);
    QCOMPARE(session->trackedDuration, seconds{0});
    QVERIFY(!session->endTimestamp.has_value());
    QCOMPARE(session->startTimestamp.toUTC(), currentTime_.toUTC());

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(service_->findActiveSession()->id, session->id);
}

void SessionServiceTest::startAutomaticSession_rejectsMissingAndUntrackedGames()
{
    QVERIFY(!service_->startAutomaticSession(999999).has_value());

    const int untrackedGameId = addTrackedGame("Untracked", "/games/untracked", std::nullopt, false);
    QVERIFY(!service_->startAutomaticSession(untrackedGameId).has_value());
    QCOMPARE(activeRowCount(), 0);
}

void SessionServiceTest::startAutomaticSession_rejectsASecondActiveSession()
{
    const int firstGameId = addTrackedGame("First", "/games/first");
    const int secondGameId = addTrackedGame("Second", "/games/second");

    QVERIFY(service_->startAutomaticSession(firstGameId).has_value());
    QVERIFY(!service_->startAutomaticSession(secondGameId).has_value());

    QCOMPARE(activeRowCount(), 1);
}

void SessionServiceTest::endActiveSession_completesTheActiveSession()
{
    const int gameId = addTrackedGame("End Me", "/games/end");
    const auto started = service_->startAutomaticSession(gameId);
    QVERIFY(started.has_value());

    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(stoppedSpy.isValid());

    currentTime_ = currentTime_.addSecs(1800);

    const auto ended = service_->endActiveSession();
    QVERIFY(ended.has_value());
    QCOMPARE(ended->id, started->id);
    QCOMPARE(ended->status, SessionStatus::Completed);
    QVERIFY(ended->endTimestamp.has_value());
    QCOMPARE(ended->endTimestamp->toUTC(), currentTime_.toUTC());
    QCOMPARE(ended->trackedDuration, seconds{1800});

    QCOMPARE(stoppedSpy.count(), 1);
    QVERIFY(!service_->findActiveSession().has_value());
    QCOMPARE(activeRowCount(), 0);
}

void SessionServiceTest::endActiveSession_returnsNulloptWithoutAnActiveSession()
{
    QVERIFY(!service_->endActiveSession().has_value());
}

void SessionServiceTest::endActiveSession_replacesTrackedDurationWithWallClock()
{
    const int gameId = addSessionGame("Replace Duration");

    Session session = makeSession(gameId, currentTime_, SessionStatus::Active);
    session.trackedDuration = seconds{99999};
    QVERIFY(service_->addSession(session));

    currentTime_ = currentTime_.addSecs(600);

    const auto ended = service_->endActiveSession();
    QVERIFY(ended.has_value());

    // The wall-clock difference replaces the prior value; it is not added to it.
    QCOMPARE(ended->trackedDuration, seconds{600});
    QCOMPARE(reloadSession(session.id)->trackedDuration, seconds{600});
}

void SessionServiceTest::endActiveSession_failsWithoutTouchingPersistenceForAnInvalidClock()
{
    const int gameId = addTrackedGame("Invalid Clock", "/games/invalid-clock");
    const auto started = service_->startAutomaticSession(gameId);
    QVERIFY(started.has_value());

    currentTime_ = QDateTime{};

    QVERIFY(!service_->endActiveSession().has_value());

    const auto persisted = reloadSession(started->id);
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->status, SessionStatus::Active);
    QVERIFY(!persisted->endTimestamp.has_value());
    QCOMPARE(activeRowCount(), 1);
}

void SessionServiceTest::endActiveSession_failsWithoutTouchingPersistenceForAnEarlierClock()
{
    const int gameId = addTrackedGame("Backwards Clock", "/games/backwards");
    const auto started = service_->startAutomaticSession(gameId);
    QVERIFY(started.has_value());

    currentTime_ = currentTime_.addSecs(-60);

    QVERIFY(!service_->endActiveSession().has_value());

    const auto persisted = reloadSession(started->id);
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->status, SessionStatus::Active);
    QVERIFY(!persisted->endTimestamp.has_value());
}

void SessionServiceTest::restoreActiveSession_succeedsWithNoActiveRows()
{
    const int gameId = addSessionGame("No Active Rows");

    Session completed = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Completed);
    QVERIFY(service_->addSession(completed));

    QVERIFY(service_->restoreActiveSession());
    QVERIFY(!service_->findActiveSession().has_value());
}

void SessionServiceTest::restoreActiveSession_restoresTheSingleActiveRow()
{
    const int gameId = addTrackedGame("Restore Me", "/games/restore");

    Session active = makeSession(gameId, utcDateTime(2026, 5, 1, 9, 0, 0), SessionStatus::Active);
    QVERIFY(service_->addSession(active));

    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(stoppedSpy.isValid());

    QVERIFY(service_->restoreActiveSession());

    const auto restored = service_->findActiveSession();
    QVERIFY(restored.has_value());
    QCOMPARE(restored->id, active.id);
    QCOMPARE(restored->gameId, gameId);
    QCOMPARE(stoppedSpy.count(), 0);
    QCOMPARE(activeRowCount(), 1);
}

void SessionServiceTest::restoreActiveSession_interruptsAnOrphanedActiveRow()
{
    const int gameId = addSessionGame("Doomed Game");

    Session active = makeSession(gameId, currentTime_.addSecs(-3600), SessionStatus::Active);
    QVERIFY(service_->addSession(active));

    // sessions.game_id cascades on delete, so the orphaned state is only
    // reachable with foreign keys temporarily disabled. This is a fixture-only
    // mechanism, per CONTRACT_CHANGES.md.
    //
    // Enforcement stays off across the repair as well: UPDATE re-validates the
    // child row's foreign key, so writing the interrupted status back would be
    // rejected with "FOREIGN KEY constraint failed" while the orphan's game is
    // still missing. The orphaned row and its repair are one indivisible
    // fixture-only condition.
    QSqlQuery pragmaOff{manager_->database()};
    QVERIFY(pragmaOff.exec("PRAGMA foreign_keys = OFF"));

    QSqlQuery deleteGame{manager_->database()};
    deleteGame.prepare("DELETE FROM games WHERE id = :id");
    deleteGame.bindValue(":id", gameId);
    QVERIFY(deleteGame.exec());

    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(stoppedSpy.isValid());

    QVERIFY(service_->restoreActiveSession());
    QVERIFY(!service_->findActiveSession().has_value());
    QCOMPARE(stoppedSpy.count(), 1);

    const auto repaired = reloadSession(active.id);
    QVERIFY(repaired.has_value());
    QCOMPARE(repaired->status, SessionStatus::Interrupted);
    QVERIFY(repaired->endTimestamp.has_value());
    QVERIFY(*repaired->endTimestamp >= repaired->startTimestamp);
    QCOMPARE(repaired->trackedDuration, seconds{3600});
    QCOMPARE(activeRowCount(), 0);

    QSqlQuery pragmaOn{manager_->database()};
    QVERIFY(pragmaOn.exec("PRAGMA foreign_keys = ON"));
}

void SessionServiceTest::updateAutomaticTracking_startsASessionAfterTheStartGracePeriod()
{
    const int gameId = addTrackedGame("Grace Game", "/games/grace");
    const std::vector<ProcessInfo> processes{makeProcess(100, "/games/grace")};

    const QSignalSpy startedSpy{service_.get(), &SessionService::sessionStarted};
    QVERIFY(startedSpy.isValid());

    service_->updateAutomaticTracking(processes, seconds{10});
    QVERIFY(!service_->findActiveSession().has_value());

    service_->updateAutomaticTracking(processes, seconds{10});
    QVERIFY(!service_->findActiveSession().has_value());

    // The third tick reaches the 30 second start grace period.
    service_->updateAutomaticTracking(processes, seconds{10});

    const auto active = service_->findActiveSession();
    QVERIFY(active.has_value());
    QCOMPARE(active->gameId, gameId);
    QCOMPARE(startedSpy.count(), 1);
}

void SessionServiceTest::updateAutomaticTracking_endsASessionAfterTheEndGracePeriod()
{
    const int gameId = addTrackedGame("Closing Game", "/games/closing");
    const std::vector<ProcessInfo> running{makeProcess(100, "/games/closing")};
    const std::vector<ProcessInfo> gone{makeProcess(101, "/usr/bin/editor")};

    const auto started = service_->startAutomaticSession(gameId);
    QVERIFY(started.has_value());

    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(stoppedSpy.isValid());

    // While the game is still detected the end timer stays reset.
    currentTime_ = currentTime_.addSecs(60);
    service_->updateAutomaticTracking(running, seconds{60});
    QVERIFY(service_->findActiveSession().has_value());

    currentTime_ = currentTime_.addSecs(20);
    service_->updateAutomaticTracking(gone, seconds{20});
    QVERIFY(service_->findActiveSession().has_value());

    currentTime_ = currentTime_.addSecs(20);
    service_->updateAutomaticTracking(gone, seconds{20});

    QVERIFY(!service_->findActiveSession().has_value());
    QCOMPARE(stoppedSpy.count(), 1);

    const auto persisted = reloadSession(started->id);
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->status, SessionStatus::Completed);
    QCOMPARE(persisted->trackedDuration, seconds{100});
}

void SessionServiceTest::updateAutomaticTracking_ignoresNonPositiveElapsed()
{
    addTrackedGame("Zero Elapsed", "/games/zero");
    const std::vector<ProcessInfo> processes{makeProcess(100, "/games/zero")};

    for(int tick = 0; tick < 10; ++tick)
    {
        service_->updateAutomaticTracking(processes, seconds{0});
        service_->updateAutomaticTracking(processes, seconds{-30});
    }

    QVERIFY(!service_->findActiveSession().has_value());
    QCOMPARE(activeRowCount(), 0);
}

void SessionServiceTest::updateAutomaticTracking_retainsAStillDetectedPendingGame()
{
    const int pathGameId = addTrackedGame("Path Candidate", "/games/path-candidate");
    addTrackedGame("Steam Candidate", "/games/steam-candidate", 4242);

    const std::vector<ProcessInfo> pathOnly{makeProcess(100, "/games/path-candidate")};
    const std::vector<ProcessInfo> both{
        makeProcess(100, "/games/path-candidate"), makeProcess(101, "/games/steam-candidate", 4242U)
    };

    // The path-only game becomes pending first.
    service_->updateAutomaticTracking(pathOnly, seconds{10});

    // A higher-priority Steam candidate appears, but the pending game is still
    // detected and must be retained rather than restarting the grace period.
    service_->updateAutomaticTracking(both, seconds{10});
    service_->updateAutomaticTracking(both, seconds{10});

    const auto active = service_->findActiveSession();
    QVERIFY(active.has_value());
    QCOMPARE(active->gameId, pathGameId);
}

void SessionServiceTest::updateAutomaticTracking_prefersSteamIdentityOverPathIdentity()
{
    // The path-only game has the lower ID, so only Steam priority can win.
    addTrackedGame("Path First", "/games/path-first");
    const int steamGameId = addTrackedGame("Steam Second", "/games/steam-second", 777);

    const std::vector<ProcessInfo> processes{
        makeProcess(100, "/games/path-first"), makeProcess(101, "/games/steam-second", 777U)
    };

    service_->updateAutomaticTracking(processes, seconds{30});

    const auto active = service_->findActiveSession();
    QVERIFY(active.has_value());
    QCOMPARE(active->gameId, steamGameId);
}

void SessionServiceTest::updateAutomaticTracking_prefersTheLowerGameIdOnATie()
{
    const int firstGameId = addTrackedGame("Tie First", "/games/tie-first");
    const int secondGameId = addTrackedGame("Tie Second", "/games/tie-second");
    QVERIFY(firstGameId < secondGameId);

    const std::vector<ProcessInfo> processes{
        makeProcess(101, "/games/tie-second"), makeProcess(100, "/games/tie-first")
    };

    service_->updateAutomaticTracking(processes, seconds{30});

    const auto active = service_->findActiveSession();
    QVERIFY(active.has_value());
    QCOMPARE(active->gameId, firstGameId);
}

void SessionServiceTest::updateAutomaticTracking_ignoresUntrackedGames()
{
    addTrackedGame("Not Tracked", "/games/not-tracked", std::nullopt, false);
    const std::vector<ProcessInfo> processes{makeProcess(100, "/games/not-tracked")};

    for(int tick = 0; tick < 5; ++tick) { service_->updateAutomaticTracking(processes, seconds{30}); }

    QVERIFY(!service_->findActiveSession().has_value());
    QCOMPARE(activeRowCount(), 0);
}

void SessionServiceTest::resetAutomaticTracking_clearsPendingStartState()
{
    const int gameId = addTrackedGame("Reset Game", "/games/reset");
    const std::vector<ProcessInfo> processes{makeProcess(100, "/games/reset")};

    service_->updateAutomaticTracking(processes, seconds{20});
    service_->resetAutomaticTracking();

    // The accumulated grace period is discarded, so 20 more seconds are not enough.
    service_->updateAutomaticTracking(processes, seconds{20});
    QVERIFY(!service_->findActiveSession().has_value());

    service_->updateAutomaticTracking(processes, seconds{20});
    const auto active = service_->findActiveSession();
    QVERIFY(active.has_value());
    QCOMPARE(active->gameId, gameId);
}

void SessionServiceTest::addSession_rejectsASecondActiveSession()
{
    const int firstGameId = addSessionGame("Active One");
    const int secondGameId = addSessionGame("Active Two");

    Session first = makeSession(firstGameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Active);
    QVERIFY(service_->addSession(first));

    Session second = makeSession(secondGameId, utcDateTime(2026, 1, 2, 8, 0, 0), SessionStatus::Active);
    QVERIFY(!service_->addSession(second));

    // The rejection happens in the service layer, so no ID was ever assigned.
    QCOMPARE(second.id, 0);
    QCOMPARE(activeRowCount(), 1);
}

void SessionServiceTest::updateSession_rejectsPromotingASecondSessionToActive()
{
    const int firstGameId = addSessionGame("Promote One");
    const int secondGameId = addSessionGame("Promote Two");

    Session active = makeSession(firstGameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Active);
    QVERIFY(service_->addSession(active));

    Session completed = makeSession(secondGameId, utcDateTime(2026, 1, 2, 8, 0, 0), SessionStatus::Completed);
    QVERIFY(service_->addSession(completed));

    Session promoted = completed;
    promoted.status = SessionStatus::Active;
    promoted.endTimestamp.reset();

    QVERIFY(!service_->updateSession(promoted));

    QCOMPARE(activeRowCount(), 1);
    QCOMPARE(reloadSession(completed.id)->status, SessionStatus::Completed);
}

void SessionServiceTest::addSession_emitsSessionStartedForAnActiveSession()
{
    const int gameId = addSessionGame("Started Game");

    const QSignalSpy startedSpy{service_.get(), &SessionService::sessionStarted};
    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(startedSpy.isValid());
    QVERIFY(stoppedSpy.isValid());

    Session session = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Active);
    QVERIFY(service_->addSession(session));

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(stoppedSpy.count(), 0);
    QCOMPARE(startedSpy.at(0).at(0).value<Game>().id, gameId);
}

void SessionServiceTest::addSession_emitsNoLifecycleSignalForAnInactiveSession()
{
    const int gameId = addSessionGame("Quiet Game");

    const QSignalSpy startedSpy{service_.get(), &SessionService::sessionStarted};
    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(startedSpy.isValid());
    QVERIFY(stoppedSpy.isValid());

    Session session = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Completed);
    QVERIFY(service_->addSession(session));

    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(stoppedSpy.count(), 0);
}

void SessionServiceTest::updateSession_emitsSessionStoppedWhenLeavingActive()
{
    const int gameId = addSessionGame("Stopping Game");

    Session session = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Active);
    QVERIFY(service_->addSession(session));

    const QSignalSpy startedSpy{service_.get(), &SessionService::sessionStarted};
    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(stoppedSpy.isValid());

    session.status = SessionStatus::Completed;
    session.endTimestamp = session.startTimestamp.addSecs(600);
    QVERIFY(service_->updateSession(session));

    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(stoppedSpy.at(0).at(0).value<Session>().id, session.id);
    QVERIFY(!service_->findActiveSession().has_value());
}

void SessionServiceTest::updateSession_emitsNoSignalForActiveToActiveEdits()
{
    const int gameId = addSessionGame("Still Active");

    Session session = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Active);
    QVERIFY(service_->addSession(session));

    const QSignalSpy startedSpy{service_.get(), &SessionService::sessionStarted};
    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(startedSpy.isValid());
    QVERIFY(stoppedSpy.isValid());

    session.trackedDuration = seconds{900};
    session.notes = "still running";
    QVERIFY(service_->updateSession(session));

    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(stoppedSpy.count(), 0);
    QCOMPARE(reloadSession(session.id)->trackedDuration, seconds{900});
}

void SessionServiceTest::updateSession_emitsNoSignalForInactiveToInactiveEdits()
{
    const int gameId = addSessionGame("Still Inactive");

    Session session = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Completed);
    QVERIFY(service_->addSession(session));

    const QSignalSpy startedSpy{service_.get(), &SessionService::sessionStarted};
    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(startedSpy.isValid());
    QVERIFY(stoppedSpy.isValid());

    session.status = SessionStatus::Interrupted;
    session.notes = "recategorized";
    QVERIFY(service_->updateSession(session));

    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(stoppedSpy.count(), 0);
}

void SessionServiceTest::updateSession_emitsSessionStartedWhenBecomingActive()
{
    const int gameId = addSessionGame("Becoming Active");

    Session session = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Completed);
    QVERIFY(service_->addSession(session));

    const QSignalSpy startedSpy{service_.get(), &SessionService::sessionStarted};
    QVERIFY(startedSpy.isValid());

    session.status = SessionStatus::Active;
    session.endTimestamp.reset();
    QVERIFY(service_->updateSession(session));

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(startedSpy.at(0).at(0).value<Game>().id, gameId);
    QCOMPARE(service_->findActiveSession()->id, session.id);
}

void SessionServiceTest::removeSession_rejectsAnActiveSession()
{
    const int gameId = addSessionGame("Cannot Remove");

    Session session = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Active);
    QVERIFY(service_->addSession(session));

    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(stoppedSpy.isValid());

    QVERIFY(!service_->removeSession(session.id));

    QCOMPARE(stoppedSpy.count(), 0);
    QVERIFY(reloadSession(session.id).has_value());
    QCOMPARE(activeRowCount(), 1);
}

void SessionServiceTest::removeSession_emitsNoLifecycleSignalForInactiveRows()
{
    const int gameId = addSessionGame("Removable");

    Session session = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Completed);
    QVERIFY(service_->addSession(session));

    const QSignalSpy startedSpy{service_.get(), &SessionService::sessionStarted};
    const QSignalSpy stoppedSpy{service_.get(), &SessionService::sessionStopped};
    QVERIFY(startedSpy.isValid());
    QVERIFY(stoppedSpy.isValid());

    QVERIFY(service_->removeSession(session.id));

    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(stoppedSpy.count(), 0);
    QVERIFY(!reloadSession(session.id).has_value());
}

void SessionServiceTest::sessionStopped_deliversAModifiableSessionCopy()
{
    const int gameId = addSessionGame("Copy Semantics");

    Session session = makeSession(gameId, utcDateTime(2026, 1, 1, 8, 0, 0), SessionStatus::Active);
    session.notes = "original";
    QVERIFY(service_->addSession(session));

    // The payload is a value: a slot may edit its own copy, but only an explicit
    // persist makes that edit durable. Signal delivery is not an in/out hook.
    connect(service_.get(),
            &SessionService::sessionStopped,
            this,
            [this](Session endedSession)
            {
                endedSession.notes = "annotated by slot";
                QVERIFY(service_->updateSession(endedSession));
            });

    Session completed = session;
    completed.status = SessionStatus::Completed;
    completed.endTimestamp = completed.startTimestamp.addSecs(600);
    QVERIFY(service_->updateSession(completed));

    // The emitter's own Session was not mutated by the slot.
    QCOMPARE(completed.notes, QString("original"));

    // The slot's explicit persist is what changed the stored row.
    const auto persisted = reloadSession(session.id);
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->notes, QString("annotated by slot"));
    QCOMPARE(persisted->status, SessionStatus::Completed);
}

void SessionServiceTest::session_isADeclaredQtMetatype()
{
    // sessionStopped carries Session by value, which requires a metatype for
    // queued connections and QSignalSpy.
    QVERIFY(QMetaType::fromType<Session>().isRegistered());

    Session session;
    session.id = 11;
    session.gameId = 22;
    session.notes = "payload";

    const QVariant boxed = QVariant::fromValue(session);
    QVERIFY(boxed.canConvert<Session>());

    const Session unboxed = boxed.value<Session>();
    QCOMPARE(unboxed.id, 11);
    QCOMPARE(unboxed.gameId, 22);
    QCOMPARE(unboxed.notes, QString("payload"));
}

int SessionServiceTest::addTrackedGame(const QString& title,
                                       const QString& executablePath,
                                       const std::optional<int> steamAppId,
                                       const bool trackingEnabled) const
{
    Game game;
    game.title = title;
    game.executablePath = executablePath;
    game.executableName = executablePath.section(QLatin1Char('/'), -1);
    game.steamAppId = steamAppId;
    game.trackingEnabled = trackingEnabled;

    if(!game_service_->addGame(game)) { return 0; }

    return game.id;
}

int SessionServiceTest::activeRowCount() const
{
    QSqlQuery query{manager_->database()};

    if(!query.exec("SELECT COUNT(*) FROM sessions WHERE status = 'active'") || !query.next()) { return -1; }

    return query.value(0).toInt();
}

std::optional<Session> SessionServiceTest::reloadSession(const int sessionId) const
{
    SessionQuery query;
    query.ids = {sessionId};

    const auto sessions = service_->search(query);
    if(sessions.empty()) { return std::nullopt; }

    return sessions.front();
}

int SessionServiceTest::addSessionGame(const QString& title) const
{
    QSqlQuery query{manager_->database()};
    query.prepare(R"(
                INSERT INTO games
                (
                    title,
                    executable_path,
                    executable_name,
                    tracking_enabled
                )
                VALUES
                (
                    :title,
                    :executable_path,
                    :executable_name,
                    1
                )
            )");
    query.bindValue(":title", title);
    query.bindValue(":executable_path", "/games/" + title.toLower());
    query.bindValue(":executable_name", title.toLower() + ".bin");

    if(!query.exec()) { return 0; }

    return query.lastInsertId().toInt();
}

Session SessionServiceTest::makeSession(int gameId, const QDateTime& startUtc, const SessionStatus status)
{
    Session session;
    session.gameId = gameId;
    session.startTimestamp = startUtc;
    session.endTimestamp = startUtc.addSecs(120);
    switch(status)
    {
    case SessionStatus::Active:
        session.endTimestamp = std::nullopt;
        break;
    case SessionStatus::Completed:
    case SessionStatus::Interrupted:
        session.endTimestamp = startUtc.addSecs(120);
    default: ;
    }
    session.trackedDuration = std::chrono::seconds{120};
    session.source = SessionSource::Automatic;
    session.status = status;
    return session;
}

QDateTime SessionServiceTest::utcDateTime(int year, int month, int day, int hour, int minute, int second)
{
    return QDateTime{{year, month, day}, {hour, minute, second}, QTimeZone::UTC};
}

QTEST_GUILESS_MAIN(SessionServiceTest)

#include "SessionServiceTest.moc"
