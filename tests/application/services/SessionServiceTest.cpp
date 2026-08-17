#include <QSqlQuery>
#include <QTimeZone>
#include <memory>
#include <QtTest/QtTest>
#include <application/services/local/CredentialService.h>
#include <application/services/web/SteamApiService.h>

#include "../../../src/application/services/local/GameService.h"
#include "../../../src/application/services/local/SessionService.h"
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

namespace
{
    class SessionServiceTest:public QObject
    {
        Q_OBJECT

    private
        slots:

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

    private:
        int addSessionGame(const QString& title) const;

        static Session makeSession(int gameId, const QDateTime& startUtc);

        static QDateTime utcDateTime(int year, int month, int day, int hour, int minute, int second);

        QString databasePath_;
        std::unique_ptr<DatabaseManager> manager_;
        std::unique_ptr<GameRepository> game_repo_;
        std::unique_ptr<GameService> game_service_;
        std::unique_ptr<SessionRepository> service_repo_;
        std::unique_ptr<SessionService> service_;
    };
}

void SessionServiceTest::init()
{
    databasePath_ = gamelog::tests::fixtures::createFreshTestDatabasePath(QString{"session-repository-%1"}.arg(QTest::currentTestFunction()));
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("session-repository");

    manager_ = std::make_unique<DatabaseManager>(databasePath_, connectionName);
    QVERIFY(manager_->initialize());

    game_repo_ = std::make_unique<GameRepository>(manager_->database());
    gamelog::application::services::CredentialService credService{};
    gamelog::application::services::SteamApiService steamService{credService};
    game_service_ = std::make_unique<GameService>(*game_repo_, steamService);

    service_repo_ = std::make_unique<SessionRepository>(manager_->database());
    service_ = std::make_unique<SessionService>(*service_repo_, *game_service_);
}

void SessionServiceTest::cleanup()
{
    game_repo_.reset();
    service_repo_.reset();
    game_service_.reset();
    service_.reset();
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

    Session session = makeSession(gameId, utcDateTime(2026, 7, 1, 12, 34, 56));
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

    Session oldest = makeSession(gameOneId, utcDateTime(2026, 1, 1, 8, 0, 0));
    oldest.status = SessionStatus::Completed;
    oldest.endTimestamp = utcDateTime(2026, 1, 1, 9, 0, 0);

    Session newest = makeSession(gameOneId, utcDateTime(2026, 1, 2, 8, 0, 0));
    newest.status = SessionStatus::Interrupted;
    newest.endTimestamp = utcDateTime(2026, 1, 2, 8, 30, 0);

    Session otherGame = makeSession(gameTwoId, utcDateTime(2026, 1, 3, 8, 0, 0));

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

    Session session = makeSession(gameId, utcDateTime(2026, 5, 5, 18, 10, 0));
    session.source = SessionSource::Manual;
    session.status = SessionStatus::Completed;
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
    Session session = makeSession(999999, utcDateTime(2026, 6, 1, 10, 0, 0));
    QVERIFY(!service_->addSession(session));
    QCOMPARE(session.id, 0);
}

void SessionServiceTest::updateSession_persistsModifiedFields()
{
    const int gameId = addSessionGame("Terraria");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 3, 1, 11, 0, 0));
    QVERIFY(service_->addSession(session));

    session.source = SessionSource::Manual;
    session.status = SessionStatus::Completed;
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

    Session session = makeSession(gameId, utcDateTime(2026, 4, 2, 9, 15, 0));
    session.id = 999999;

    QVERIFY(!service_->updateSession(session));
}

void SessionServiceTest::removeSession_deletesExistingRow()
{
    const int gameId = addSessionGame("Delete Session");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 2, 2, 20, 0, 0));
    QVERIFY(service_->addSession(session));

    QVERIFY(service_->removeSession(session.id));
    const auto sessions = service_->listSessionsForGame(gameId);
    QVERIFY(sessions.empty());
}

void SessionServiceTest::removeSession_returnsFalseForMissingRow()
{
    QVERIFY(!service_->removeSession(999999));
}

int SessionServiceTest::addSessionGame(const QString& title) const
{
    QSqlQuery query{manager_->database()};
    query.prepare(
                  R"(
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
            )"
                 );
    query.bindValue(":title", title);
    query.bindValue(":executable_path", "/games/" + title.toLower());
    query.bindValue(":executable_name", title.toLower() + ".bin");

    if(!query.exec())
    {
        return 0;
    }

    return query.lastInsertId().toInt();
}

Session SessionServiceTest::makeSession(int gameId, const QDateTime& startUtc)
{
    Session session;
    session.gameId = gameId;
    session.startTimestamp = startUtc;
    session.endTimestamp.reset();
    session.trackedDuration = std::chrono::seconds{120};
    session.source = SessionSource::Automatic;
    session.status = SessionStatus::Active;
    return session;
}

QDateTime SessionServiceTest::utcDateTime(int year, int month, int day, int hour, int minute, int second)
{
    return QDateTime{{year, month, day}, {hour, minute, second}, QTimeZone::UTC};
}

QTEST_GUILESS_MAIN(SessionServiceTest)

#include "SessionServiceTest.moc"
