#include <QSqlQuery>
#include <QTimeZone>
#include <QtTest/QtTest>
#include <memory>

#include "../../fixtures/TestDatabaseFixture.h"
#include "database/DatabaseManager.h"
#include "database/SessionRepository.h"

using gamelog::core::database::DatabaseManager;
using gamelog::core::database::SessionRepository;
using gamelog::core::domain::Session;
using gamelog::core::domain::SessionSource;
using gamelog::core::domain::SessionStatus;

class SessionRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void findActiveSession_returnsNulloptWhenNoActiveSessionExists();
    void findActiveSession_returnsInsertedActiveSession();
    void listSessionsForGame_returnsNewestFirstAndFiltersByGame();
    void listSessionsForGame_returnsEmptyWhenNoRowsExist();
    void insert_persistsSessionAndAssignsId();
    void insert_failsForUnknownGameId();
    void update_persistsModifiedFields();
    void update_returnsFalseForMissingRow();
    void remove_deletesExistingRow();
    void remove_returnsFalseForMissingRow();

private:
    int insertGame(const QString &title) const;
    static Session makeSession(int gameId, const QDateTime &startUtc);
    static QDateTime utcDateTime(int year, int month, int day, int hour, int minute, int second);

    QString databasePath_;
    std::unique_ptr<DatabaseManager> manager_;
    std::unique_ptr<SessionRepository> repository_;
};

void SessionRepositoryTest::init()
{
    databasePath_ = gamelog::tests::fixtures::createFreshTestDatabasePath(
            QString{"session-repository-%1"}.arg(QTest::currentTestFunction()));
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("session-repository");

    manager_ = std::make_unique<DatabaseManager>(databasePath_, connectionName);
    QVERIFY(manager_->initialize());

    repository_ = std::make_unique<SessionRepository>(manager_->database());
}

void SessionRepositoryTest::cleanup()
{
    repository_.reset();
    manager_.reset();
    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath_);
}

int SessionRepositoryTest::insertGame(const QString &title) const
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
            )");
    query.bindValue(":title", title);
    query.bindValue(":executable_path", "/games/" + title.toLower());
    query.bindValue(":executable_name", title.toLower() + ".bin");

    if (!query.exec())
    {
        return 0;
    }

    return query.lastInsertId().toInt();
}

Session SessionRepositoryTest::makeSession(int gameId, const QDateTime &startUtc)
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

QDateTime SessionRepositoryTest::utcDateTime(
        int year, int month, int day, int hour, int minute, int second)
{
    return QDateTime{{year, month, day}, {hour, minute, second}, QTimeZone::UTC};
}

void SessionRepositoryTest::findActiveSession_returnsNulloptWhenNoActiveSessionExists()
{
    const auto active = repository_->findActiveSession();
    QVERIFY(!active.has_value());
}

void SessionRepositoryTest::findActiveSession_returnsInsertedActiveSession()
{
    const int gameId = insertGame("Elden Ring");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 7, 1, 12, 34, 56));
    QVERIFY(repository_->insert(session));

    const auto active = repository_->findActiveSession();
    QVERIFY(active.has_value());
    QCOMPARE(active->id, session.id);
    QCOMPARE(active->gameId, gameId);
    QCOMPARE(active->startTimestamp.toUTC(), session.startTimestamp.toUTC());
    QVERIFY(!active->endTimestamp.has_value());
    QCOMPARE(active->trackedDuration, std::chrono::seconds{120});
    QCOMPARE(active->source, SessionSource::Automatic);
    QCOMPARE(active->status, SessionStatus::Active);
}

void SessionRepositoryTest::listSessionsForGame_returnsNewestFirstAndFiltersByGame()
{
    const int gameOneId = insertGame("Game One");
    const int gameTwoId = insertGame("Game Two");
    QVERIFY(gameOneId > 0);
    QVERIFY(gameTwoId > 0);

    Session oldest = makeSession(gameOneId, utcDateTime(2026, 1, 1, 8, 0, 0));
    oldest.status = SessionStatus::Completed;
    oldest.endTimestamp = utcDateTime(2026, 1, 1, 9, 0, 0);

    Session newest = makeSession(gameOneId, utcDateTime(2026, 1, 2, 8, 0, 0));
    newest.status = SessionStatus::Interrupted;
    newest.endTimestamp = utcDateTime(2026, 1, 2, 8, 30, 0);

    Session otherGame = makeSession(gameTwoId, utcDateTime(2026, 1, 3, 8, 0, 0));

    QVERIFY(repository_->insert(oldest));
    QVERIFY(repository_->insert(newest));
    QVERIFY(repository_->insert(otherGame));

    const auto sessions = repository_->listSessionsForGame(gameOneId);
    QCOMPARE(sessions.size(), 2);
    QCOMPARE(sessions[0].id, newest.id);
    QCOMPARE(sessions[1].id, oldest.id);
    QCOMPARE(sessions[0].gameId, gameOneId);
    QCOMPARE(sessions[1].gameId, gameOneId);
}

void SessionRepositoryTest::listSessionsForGame_returnsEmptyWhenNoRowsExist()
{
    const int gameId = insertGame("No Sessions");
    QVERIFY(gameId > 0);

    const auto sessions = repository_->listSessionsForGame(gameId);
    QVERIFY(sessions.empty());
}

void SessionRepositoryTest::insert_persistsSessionAndAssignsId()
{
    const int gameId = insertGame("Hades");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 5, 5, 18, 10, 0));
    session.source = SessionSource::Manual;
    session.status = SessionStatus::Completed;
    session.endTimestamp = utcDateTime(2026, 5, 5, 18, 40, 0);
    session.trackedDuration = std::chrono::seconds{1800};

    QVERIFY(repository_->insert(session));
    QVERIFY(session.id > 0);

    const auto sessions = repository_->listSessionsForGame(gameId);
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions[0].id, session.id);
    QCOMPARE(sessions[0].source, SessionSource::Manual);
    QCOMPARE(sessions[0].status, SessionStatus::Completed);
    QVERIFY(sessions[0].endTimestamp.has_value());
}

void SessionRepositoryTest::insert_failsForUnknownGameId()
{
    Session session = makeSession(999999, utcDateTime(2026, 6, 1, 10, 0, 0));
    QVERIFY(!repository_->insert(session));
    QCOMPARE(session.id, 0);
}

void SessionRepositoryTest::update_persistsModifiedFields()
{
    const int gameId = insertGame("Terraria");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 3, 1, 11, 0, 0));
    QVERIFY(repository_->insert(session));

    session.source = SessionSource::Manual;
    session.status = SessionStatus::Completed;
    session.endTimestamp = utcDateTime(2026, 3, 1, 11, 45, 0);
    session.trackedDuration = std::chrono::seconds{2700};

    QVERIFY(repository_->update(session));

    const auto sessions = repository_->listSessionsForGame(gameId);
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions[0].id, session.id);
    QCOMPARE(sessions[0].source, SessionSource::Manual);
    QCOMPARE(sessions[0].status, SessionStatus::Completed);
    QVERIFY(sessions[0].endTimestamp.has_value());
    QCOMPARE(sessions[0].endTimestamp->toUTC(), session.endTimestamp->toUTC());
    QCOMPARE(sessions[0].trackedDuration, std::chrono::seconds{2700});
}

void SessionRepositoryTest::update_returnsFalseForMissingRow()
{
    const int gameId = insertGame("Missing Session");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 4, 2, 9, 15, 0));
    session.id = 999999;

    QVERIFY(!repository_->update(session));
}

void SessionRepositoryTest::remove_deletesExistingRow()
{
    const int gameId = insertGame("Delete Session");
    QVERIFY(gameId > 0);

    Session session = makeSession(gameId, utcDateTime(2026, 2, 2, 20, 0, 0));
    QVERIFY(repository_->insert(session));

    QVERIFY(repository_->remove(session.id));
    const auto sessions = repository_->listSessionsForGame(gameId);
    QVERIFY(sessions.empty());
}

void SessionRepositoryTest::remove_returnsFalseForMissingRow()
{
    QVERIFY(!repository_->remove(999999));
}

QTEST_GUILESS_MAIN(SessionRepositoryTest)

#include "SessionRepositoryTest.moc"
