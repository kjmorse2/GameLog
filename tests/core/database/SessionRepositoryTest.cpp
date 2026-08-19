#include <QtTest/QtTest>

#include "database/DatabaseManager.h"
#include "database/SessionRepository.h"
#include "domain/Session.h"
#include "domain/query/SessionQuery.h"
#include "fixtures/TestDatabaseFixture.h"
#include "fixtures/LoggingTestSupport.h"

#include <chrono>
#include <cstddef>
#include <memory>

#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>

using gamelog::core::database::DatabaseManager;
using gamelog::core::database::SessionRepository;
using gamelog::core::domain::Session;
using gamelog::core::domain::SessionSource;
using gamelog::core::domain::SessionStatus;
using gamelog::core::domain::query::SessionQuery;
using gamelog::core::domain::query::SessionSortField;
using gamelog::core::domain::query::SortDirection;
using std::chrono::seconds;

namespace
{
    QDateTime utcDateTime(const int year,
                          const int month,
                          const int day,
                          const int hour,
                          const int minute,
                          const int second,
                          const int millisecond = 0)
    {
        return QDateTime{{year, month, day}, {hour, minute, second, millisecond}, QTimeZone::UTC};
    }

    Session makeSession(const int gameId, const QDateTime& startUtc, const SessionStatus status)
    {
        Session session;
        session.gameId = gameId;
        session.startTimestamp = startUtc;
        session.trackedDuration = seconds{120};
        session.source = SessionSource::Automatic;
        session.status = status;

        if(status != SessionStatus::Active) { session.endTimestamp = startUtc.addSecs(120); }

        return session;
    }
} // namespace

namespace
{
    class SessionRepositoryTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        void cleanup();

        void insert_assignsIdAndCreatesDocumentRow();

        void query_roundTripsPersistedRow();

        void query_returnsEmptyVectorWhenNothingMatches();

        void update_persistsModifiedFields();

        void remove_deletesExistingRow();

        void remove_returnsFalseForZeroNegativeAndMissingIds();

        void query_filtersByIdsAndGameIds();

        void query_filtersBySourcesAndStatuses();

        void query_filtersByTrackedDurationBounds();

        void query_filtersByHasEndTimestamp();

        void query_appliesSortFieldAndDirection();

        void query_appliesLimitAndOffset();

        void query_appliesHalfOpenRangeToBothTimestampFormats();

        void insert_rejectsInvalidStartTimestamp();

        void insert_rejectsNegativeTrackedDuration();

        void insert_rejectsActiveSessionWithEndTimestamp();

        void insert_rejectsInactiveSessionWithoutEndTimestamp();

        void insert_rejectsEndTimestampBeforeStart();

        void insert_acceptsEqualStartAndEndTimestamps();

        void insert_rejectsPreassignedId();

        void update_rejectsNonPositiveIdAndInvalidState();

        void query_returnsSessionWithEmptyNotesWhenDocumentIsMissing();

        void insert_createsDocumentRowForEmptyNotes();

        void update_leavesDocumentTimestampUnchangedForUnrelatedEdit();

        void update_assignsStrictlyLaterDocumentTimestampForChangedNote();

        void insert_rollsBackAndRestoresZeroIdWhenDocumentInsertFails();

        void update_rollsBackSessionRowWhenDocumentUpdateFails();

        void query_skipsRowWithUnknownSource();

        void query_skipsRowWithUnknownStatus();

        void query_skipsRowWithNegativeTrackedDuration();

        void query_skipsRowWithInvalidStartTimestamp();

        void query_skipsActiveRowThatHasAnEndTimestamp();

        void query_skipsInactiveRowWhoseEndPrecedesStart();

    private:
        [[nodiscard]] int insertGame(const QString& title) const;

        [[nodiscard]] int insertRawSession(int gameId,
                                           const QString& startText,
                                           const QVariant& endText,
                                           qlonglong durationSeconds,
                                           const QString& source,
                                           const QString& status,
                                           bool ignoreCheckConstraints = false) const;

        [[nodiscard]] QVariant documentValue(int sessionId, const QString& column) const;

        [[nodiscard]] int sessionRowCount() const;

        QString databasePath_;
        std::unique_ptr<DatabaseManager> manager_;
        std::unique_ptr<SessionRepository> repository_;
        int gameId_{0};
    };
} // namespace

void SessionRepositoryTest::init()
{
    // These tests assert on logged messages, so the categories must be on
    // regardless of any ambient QT_LOGGING_RULES.
    gamelog::tests::fixtures::enableGameLogLoggingCategories();

    QTest::failOnWarning();

    databasePath_ = gamelog::tests::fixtures::createFreshTestDatabasePath(QStringLiteral("session-repository-%1").
                                                                          arg(QString::fromLatin1(QTest::currentTestFunction())));
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("session-repository");

    manager_ = std::make_unique<DatabaseManager>(databasePath_, connectionName);
    QVERIFY(manager_->initialize());

    repository_ = std::make_unique<SessionRepository>(manager_->database());

    gameId_ = insertGame(QStringLiteral("Fixture Game"));
    QVERIFY(gameId_ > 0);
}

void SessionRepositoryTest::cleanup()
{
    repository_.reset();
    manager_.reset();
    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath_);
}

int SessionRepositoryTest::insertGame(const QString& title) const
{
    QSqlQuery query{manager_->database()};
    query.prepare(QStringLiteral("INSERT INTO games (title, executable_path, executable_name, tracking_enabled) "
                                 "VALUES (:title, :executable_path, :executable_name, 1)"));
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(QStringLiteral(":executable_path"), QStringLiteral("/games/") + title.toLower());
    query.bindValue(QStringLiteral(":executable_name"), title.toLower() + QStringLiteral(".bin"));

    if(!query.exec()) { return 0; }

    return query.lastInsertId().toInt();
}

int SessionRepositoryTest::insertRawSession(const int gameId,
                                            const QString& startText,
                                            const QVariant& endText,
                                            const qlonglong durationSeconds,
                                            const QString& source,
                                            const QString& status,
                                            const bool ignoreCheckConstraints) const
{
    // Some contracted defenses guard states the CHECK constraints prevent. The
    // pragma is a fixture-only mechanism, per CONTRACT_CHANGES.md.
    if(ignoreCheckConstraints)
    {
        QSqlQuery pragma{manager_->database()};
        if(!pragma.exec(QStringLiteral("PRAGMA ignore_check_constraints = ON"))) { return 0; }
    }

    QSqlQuery query{manager_->database()};
    query.prepare(QStringLiteral("INSERT INTO sessions (game_id, start_timestamp_utc, end_timestamp_utc, "
                                 "tracked_duration_seconds, source, status) VALUES (:game_id, :start, :end, "
                                 ":duration, :source, :status)"));
    query.bindValue(QStringLiteral(":game_id"), gameId);
    query.bindValue(QStringLiteral(":start"), startText);
    query.bindValue(QStringLiteral(":end"), endText);
    query.bindValue(QStringLiteral(":duration"), durationSeconds);
    query.bindValue(QStringLiteral(":source"), source);
    query.bindValue(QStringLiteral(":status"), status);

    const bool executed = query.exec();
    const int insertedId = executed ? query.lastInsertId().toInt() : 0;

    if(ignoreCheckConstraints)
    {
        QSqlQuery pragma{manager_->database()};
        static_cast<void>(pragma.exec(QStringLiteral("PRAGMA ignore_check_constraints = OFF")));
    }

    return insertedId;
}

QVariant SessionRepositoryTest::documentValue(const int sessionId, const QString& column) const
{
    QSqlQuery query{manager_->database()};
    query.prepare(QStringLiteral("SELECT %1 FROM session_documents WHERE session_id = :session_id").arg(column));
    query.bindValue(QStringLiteral(":session_id"), sessionId);

    if(!query.exec() || !query.next()) { return {}; }

    return query.value(0);
}

int SessionRepositoryTest::sessionRowCount() const
{
    QSqlQuery query{manager_->database()};

    if(!query.exec(QStringLiteral("SELECT COUNT(*) FROM sessions")) || !query.next()) { return -1; }

    return query.value(0).toInt();
}

void SessionRepositoryTest::insert_assignsIdAndCreatesDocumentRow()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    session.notes = QStringLiteral("<p>First run.</p>");

    QVERIFY(repository_->insert(session));
    QVERIFY(session.id > 0);

    QCOMPARE(documentValue(session.id, QStringLiteral("content")).toString(), QStringLiteral("<p>First run.</p>"));
    QVERIFY(!documentValue(session.id, QStringLiteral("last_saved_timestamp_utc")).toString().isEmpty());
}

void SessionRepositoryTest::query_roundTripsPersistedRow()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Interrupted);
    session.source = SessionSource::Manual;
    session.trackedDuration = seconds{3600};
    session.endTimestamp = utcDateTime(2026, 3, 1, 13, 0, 0);
    session.notes = QStringLiteral("notes");

    QVERIFY(repository_->insert(session));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].id, session.id);
    QCOMPARE(sessions[0].gameId, gameId_);
    QCOMPARE(sessions[0].startTimestamp.toUTC(), session.startTimestamp.toUTC());
    QVERIFY(sessions[0].endTimestamp.has_value());
    QCOMPARE(sessions[0].endTimestamp->toUTC(), session.endTimestamp->toUTC());
    QCOMPARE(sessions[0].trackedDuration, seconds{3600});
    QCOMPARE(sessions[0].source, SessionSource::Manual);
    QCOMPARE(sessions[0].status, SessionStatus::Interrupted);
    QCOMPARE(sessions[0].notes, QStringLiteral("notes"));
}

void SessionRepositoryTest::query_returnsEmptyVectorWhenNothingMatches()
{
    QVERIFY(repository_->query({}).empty());

    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(session));

    SessionQuery missingId;
    missingId.ids = {999999};
    QVERIFY(repository_->query(missingId).empty());

    SessionQuery missingGame;
    missingGame.gameIds = {999999};
    QVERIFY(repository_->query(missingGame).empty());
}

void SessionRepositoryTest::update_persistsModifiedFields()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Active);
    QVERIFY(repository_->insert(session));

    session.status = SessionStatus::Completed;
    session.endTimestamp = utcDateTime(2026, 3, 1, 14, 30, 0);
    session.trackedDuration = seconds{9000};
    session.source = SessionSource::Manual;
    session.notes = QStringLiteral("updated notes");

    QVERIFY(repository_->update(session));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].status, SessionStatus::Completed);
    QCOMPARE(sessions[0].source, SessionSource::Manual);
    QCOMPARE(sessions[0].trackedDuration, seconds{9000});
    QCOMPARE(sessions[0].notes, QStringLiteral("updated notes"));
}

void SessionRepositoryTest::remove_deletesExistingRow()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(session));

    QVERIFY(repository_->remove(session.id));
    QVERIFY(repository_->query({}).empty());
    QCOMPARE(sessionRowCount(), 0);
}

void SessionRepositoryTest::remove_returnsFalseForZeroNegativeAndMissingIds()
{
    QVERIFY(!repository_->remove(0));
    QVERIFY(!repository_->remove(-1));
    QVERIFY(!repository_->remove(999999));
}

void SessionRepositoryTest::query_filtersByIdsAndGameIds()
{
    const int otherGameId = insertGame(QStringLiteral("Other Game"));
    QVERIFY(otherGameId > 0);

    Session first = makeSession(gameId_, utcDateTime(2026, 3, 1, 8, 0, 0), SessionStatus::Completed);
    Session second = makeSession(gameId_, utcDateTime(2026, 3, 2, 8, 0, 0), SessionStatus::Completed);
    Session third = makeSession(otherGameId, utcDateTime(2026, 3, 3, 8, 0, 0), SessionStatus::Completed);

    QVERIFY(repository_->insert(first));
    QVERIFY(repository_->insert(second));
    QVERIFY(repository_->insert(third));

    SessionQuery singleId;
    singleId.ids = {second.id};
    QCOMPARE(static_cast<int>(repository_->query(singleId).size()), 1);

    SessionQuery twoIds;
    twoIds.ids = {first.id, third.id};
    QCOMPARE(static_cast<int>(repository_->query(twoIds).size()), 2);

    SessionQuery byGame;
    byGame.gameIds = {gameId_};
    QCOMPARE(static_cast<int>(repository_->query(byGame).size()), 2);

    SessionQuery byBothGames;
    byBothGames.gameIds = {gameId_, otherGameId};
    QCOMPARE(static_cast<int>(repository_->query(byBothGames).size()), 3);
}

void SessionRepositoryTest::query_filtersBySourcesAndStatuses()
{
    Session automaticCompleted = makeSession(gameId_, utcDateTime(2026, 3, 1, 8, 0, 0), SessionStatus::Completed);

    Session manualInterrupted = makeSession(gameId_, utcDateTime(2026, 3, 2, 8, 0, 0), SessionStatus::Interrupted);
    manualInterrupted.source = SessionSource::Manual;

    Session automaticActive = makeSession(gameId_, utcDateTime(2026, 3, 3, 8, 0, 0), SessionStatus::Active);

    QVERIFY(repository_->insert(automaticCompleted));
    QVERIFY(repository_->insert(manualInterrupted));
    QVERIFY(repository_->insert(automaticActive));

    SessionQuery automaticOnly;
    automaticOnly.sources = {SessionSource::Automatic};
    QCOMPARE(static_cast<int>(repository_->query(automaticOnly).size()), 2);

    SessionQuery manualOnly;
    manualOnly.sources = {SessionSource::Manual};
    QCOMPARE(static_cast<int>(repository_->query(manualOnly).size()), 1);

    SessionQuery bothSources;
    bothSources.sources = {SessionSource::Automatic, SessionSource::Manual};
    QCOMPARE(static_cast<int>(repository_->query(bothSources).size()), 3);

    SessionQuery activeOnly;
    activeOnly.statuses = {SessionStatus::Active};
    const std::vector<Session> active = repository_->query(activeOnly);
    QCOMPARE(static_cast<int>(active.size()), 1);
    QCOMPARE(active[0].id, automaticActive.id);

    SessionQuery inactiveOnly;
    inactiveOnly.statuses = {SessionStatus::Completed, SessionStatus::Interrupted};
    QCOMPARE(static_cast<int>(repository_->query(inactiveOnly).size()), 2);
}

void SessionRepositoryTest::query_filtersByTrackedDurationBounds()
{
    Session shortSession = makeSession(gameId_, utcDateTime(2026, 3, 1, 8, 0, 0), SessionStatus::Completed);
    shortSession.trackedDuration = seconds{60};

    Session mediumSession = makeSession(gameId_, utcDateTime(2026, 3, 2, 8, 0, 0), SessionStatus::Completed);
    mediumSession.trackedDuration = seconds{600};

    Session longSession = makeSession(gameId_, utcDateTime(2026, 3, 3, 8, 0, 0), SessionStatus::Completed);
    longSession.trackedDuration = seconds{6000};

    QVERIFY(repository_->insert(shortSession));
    QVERIFY(repository_->insert(mediumSession));
    QVERIFY(repository_->insert(longSession));

    SessionQuery minimum;
    minimum.minimumTrackedDuration = seconds{600};
    QCOMPARE(static_cast<int>(repository_->query(minimum).size()), 2);

    SessionQuery maximum;
    maximum.maximumTrackedDuration = seconds{600};
    QCOMPARE(static_cast<int>(repository_->query(maximum).size()), 2);

    SessionQuery band;
    band.minimumTrackedDuration = seconds{600};
    band.maximumTrackedDuration = seconds{600};
    const std::vector<Session> exact = repository_->query(band);
    QCOMPARE(static_cast<int>(exact.size()), 1);
    QCOMPARE(exact[0].id, mediumSession.id);

    SessionQuery empty;
    empty.minimumTrackedDuration = seconds{100000};
    QVERIFY(repository_->query(empty).empty());
}

void SessionRepositoryTest::query_filtersByHasEndTimestamp()
{
    Session completed = makeSession(gameId_, utcDateTime(2026, 3, 1, 8, 0, 0), SessionStatus::Completed);
    Session active = makeSession(gameId_, utcDateTime(2026, 3, 2, 8, 0, 0), SessionStatus::Active);

    QVERIFY(repository_->insert(completed));
    QVERIFY(repository_->insert(active));

    SessionQuery withEnd;
    withEnd.hasEndTimestamp = true;
    const std::vector<Session> ended = repository_->query(withEnd);
    QCOMPARE(static_cast<int>(ended.size()), 1);
    QCOMPARE(ended[0].id, completed.id);

    SessionQuery withoutEnd;
    withoutEnd.hasEndTimestamp = false;
    const std::vector<Session> running = repository_->query(withoutEnd);
    QCOMPARE(static_cast<int>(running.size()), 1);
    QCOMPARE(running[0].id, active.id);
}

void SessionRepositoryTest::query_appliesSortFieldAndDirection()
{
    Session first = makeSession(gameId_, utcDateTime(2026, 3, 1, 8, 0, 0), SessionStatus::Completed);
    first.trackedDuration = seconds{300};

    Session second = makeSession(gameId_, utcDateTime(2026, 3, 2, 8, 0, 0), SessionStatus::Completed);
    second.trackedDuration = seconds{100};

    Session third = makeSession(gameId_, utcDateTime(2026, 3, 3, 8, 0, 0), SessionStatus::Completed);
    third.trackedDuration = seconds{200};

    QVERIFY(repository_->insert(first));
    QVERIFY(repository_->insert(second));
    QVERIFY(repository_->insert(third));

    SessionQuery startAscending;
    startAscending.sortBy = SessionSortField::StartTimestamp;
    startAscending.sortDirection = SortDirection::Ascending;
    const std::vector<Session> byStart = repository_->query(startAscending);
    QCOMPARE(byStart[0].id, first.id);
    QCOMPARE(byStart[2].id, third.id);

    SessionQuery startDescending;
    startDescending.sortBy = SessionSortField::StartTimestamp;
    startDescending.sortDirection = SortDirection::Descending;
    QCOMPARE(repository_->query(startDescending)[0].id, third.id);

    SessionQuery durationAscending;
    durationAscending.sortBy = SessionSortField::TrackedDuration;
    durationAscending.sortDirection = SortDirection::Ascending;
    const std::vector<Session> byDuration = repository_->query(durationAscending);
    QCOMPARE(byDuration[0].id, second.id);
    QCOMPARE(byDuration[2].id, first.id);

    SessionQuery idDescending;
    idDescending.sortBy = SessionSortField::Id;
    idDescending.sortDirection = SortDirection::Descending;
    const std::vector<Session> byId = repository_->query(idDescending);
    QCOMPARE(byId[0].id, third.id);
    QCOMPARE(byId[2].id, first.id);
}

void SessionRepositoryTest::query_appliesLimitAndOffset()
{
    Session first = makeSession(gameId_, utcDateTime(2026, 3, 1, 8, 0, 0), SessionStatus::Completed);
    Session second = makeSession(gameId_, utcDateTime(2026, 3, 2, 8, 0, 0), SessionStatus::Completed);
    Session third = makeSession(gameId_, utcDateTime(2026, 3, 3, 8, 0, 0), SessionStatus::Completed);

    QVERIFY(repository_->insert(first));
    QVERIFY(repository_->insert(second));
    QVERIFY(repository_->insert(third));

    SessionQuery ascending;
    ascending.sortBy = SessionSortField::Id;
    ascending.sortDirection = SortDirection::Ascending;

    SessionQuery zeroLimit = ascending;
    zeroLimit.limit = std::size_t{0};
    QVERIFY(repository_->query(zeroLimit).empty());

    SessionQuery limited = ascending;
    limited.limit = std::size_t{2};
    QCOMPARE(static_cast<int>(repository_->query(limited).size()), 2);

    SessionQuery offsetOnly = ascending;
    offsetOnly.offset = std::size_t{1};
    const std::vector<Session> skipped = repository_->query(offsetOnly);
    QCOMPARE(static_cast<int>(skipped.size()), 2);
    QCOMPARE(skipped[0].id, second.id);

    SessionQuery offsetPastEnd = ascending;
    offsetPastEnd.offset = std::size_t{25};
    QVERIFY(repository_->query(offsetPastEnd).empty());

    SessionQuery limitAndOffset = ascending;
    limitAndOffset.limit = std::size_t{1};
    limitAndOffset.offset = std::size_t{2};
    const std::vector<Session> last = repository_->query(limitAndOffset);
    QCOMPARE(static_cast<int>(last.size()), 1);
    QCOMPARE(last[0].id, third.id);
}

void SessionRepositoryTest::query_appliesHalfOpenRangeToBothTimestampFormats()
{
    // The repository writes Qt::ISODateWithMs. A legacy row written without
    // milliseconds sorts above the same instant under raw TEXT comparison
    // ('Z' > '.'), which is exactly the misclassification this contract forbids.
    const int legacyRowId = insertRawSession(gameId_,
                                             QStringLiteral("2026-03-01T12:00:00Z"),
                                             QVariant{QStringLiteral("2026-03-01T13:00:00Z")},
                                             3600,
                                             QStringLiteral("automatic"),
                                             QStringLiteral("completed"));
    QVERIFY(legacyRowId > 0);

    Session millisecondRow = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(millisecondRow));

    const auto sessionsInRange = [this](const QDateTime& start, const QDateTime& end)
    {
        SessionQuery query;
        query.startedAtOrAfter = start;
        query.startedBefore = end;
        return repository_->query(query);
    };

    // Lower bound is inclusive for both persisted representations.
    QCOMPARE(static_cast<int>(sessionsInRange(utcDateTime(2026, 3, 1, 12, 0, 0), utcDateTime(2026, 3, 1, 13, 0, 0)).
                 size()),
             2);

    // One millisecond past the start excludes both rows. Raw TEXT comparison
    // would have kept the no-millisecond row here.
    QVERIFY(sessionsInRange(utcDateTime(2026, 3, 1, 12, 0, 0, 1), utcDateTime(2026, 3, 1, 13, 0, 0)).empty());

    // Upper bound is exclusive at exactly the start instant.
    QVERIFY(sessionsInRange(utcDateTime(2026, 3, 1, 11, 0, 0), utcDateTime(2026, 3, 1, 12, 0, 0)).empty());

    // One millisecond past the start includes both rows again.
    QCOMPARE(static_cast<int>(sessionsInRange(utcDateTime(2026, 3, 1, 11, 0, 0), utcDateTime(2026, 3, 1, 12, 0, 0, 1)).
                 size()),
             2);

    // A lower bound alone behaves the same way for both representations.
    SessionQuery lowerOnly;
    lowerOnly.startedAtOrAfter = utcDateTime(2026, 3, 1, 12, 0, 0);
    QCOMPARE(static_cast<int>(repository_->query(lowerOnly).size()), 2);

    SessionQuery upperOnly;
    upperOnly.startedBefore = utcDateTime(2026, 3, 1, 12, 0, 0);
    QVERIFY(repository_->query(upperOnly).empty());
}

void SessionRepositoryTest::insert_rejectsInvalidStartTimestamp()
{
    Session session = makeSession(gameId_, QDateTime{}, SessionStatus::Active);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*start timestamp is invalid.*"));
    QVERIFY(!repository_->insert(session));
    QCOMPARE(session.id, 0);
    QCOMPARE(sessionRowCount(), 0);
}

void SessionRepositoryTest::insert_rejectsNegativeTrackedDuration()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    session.trackedDuration = seconds{-1};

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*tracked duration is negative.*"));
    QVERIFY(!repository_->insert(session));
    QCOMPARE(sessionRowCount(), 0);
}

void SessionRepositoryTest::insert_rejectsActiveSessionWithEndTimestamp()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Active);
    session.endTimestamp = utcDateTime(2026, 3, 1, 13, 0, 0);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*active session has an end timestamp.*"));
    QVERIFY(!repository_->insert(session));
    QCOMPARE(sessionRowCount(), 0);
}

void SessionRepositoryTest::insert_rejectsInactiveSessionWithoutEndTimestamp()
{
    for(const SessionStatus status : {SessionStatus::Completed, SessionStatus::Interrupted})
    {
        Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), status);
        session.endTimestamp.reset();

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*lacks a valid end timestamp.*"));
        QVERIFY(!repository_->insert(session));
    }

    QCOMPARE(sessionRowCount(), 0);
}

void SessionRepositoryTest::insert_rejectsEndTimestampBeforeStart()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    session.endTimestamp = utcDateTime(2026, 3, 1, 11, 59, 59);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*end timestamp precedes start timestamp.*"));
    QVERIFY(!repository_->insert(session));
    QCOMPARE(sessionRowCount(), 0);
}

void SessionRepositoryTest::insert_acceptsEqualStartAndEndTimestamps()
{
    const QDateTime instant = utcDateTime(2026, 3, 1, 12, 0, 0);

    Session session = makeSession(gameId_, instant, SessionStatus::Completed);
    session.endTimestamp = instant;
    session.trackedDuration = seconds{0};

    QVERIFY(repository_->insert(session));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].startTimestamp.toUTC(), sessions[0].endTimestamp->toUTC());
    QCOMPARE(sessions[0].trackedDuration, seconds{0});
}

void SessionRepositoryTest::insert_rejectsPreassignedId()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    session.id = 17;

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*already has an ID.*"));
    QVERIFY(!repository_->insert(session));
    QCOMPARE(sessionRowCount(), 0);
}

void SessionRepositoryTest::update_rejectsNonPositiveIdAndInvalidState()
{
    Session persisted = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(persisted));

    Session withoutId = persisted;
    withoutId.id = 0;
    QVERIFY(!repository_->update(withoutId));

    Session negativeId = persisted;
    negativeId.id = -5;
    QVERIFY(!repository_->update(negativeId));

    Session invalidState = persisted;
    invalidState.trackedDuration = seconds{-30};
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*tracked duration is negative.*"));
    QVERIFY(!repository_->update(invalidState));

    Session missingRow = persisted;
    missingRow.id = 999999;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Failed to update session.*"));
    QVERIFY(!repository_->update(missingRow));

    // None of the rejected updates may have altered the persisted row.
    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].trackedDuration, seconds{120});
}

void SessionRepositoryTest::query_returnsSessionWithEmptyNotesWhenDocumentIsMissing()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    session.notes = QStringLiteral("will be orphaned");
    QVERIFY(repository_->insert(session));

    QSqlQuery deleteDocument{manager_->database()};
    deleteDocument.prepare(QStringLiteral("DELETE FROM session_documents WHERE session_id = :session_id"));
    deleteDocument.bindValue(QStringLiteral(":session_id"), session.id);
    QVERIFY(deleteDocument.exec());

    // The left join must keep the session visible rather than hiding it.
    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].id, session.id);
    QVERIFY(sessions[0].notes.isEmpty());
}

void SessionRepositoryTest::insert_createsDocumentRowForEmptyNotes()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    session.notes.clear();

    QVERIFY(repository_->insert(session));

    QSqlQuery query{manager_->database()};
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM session_documents WHERE session_id = :session_id"));
    query.bindValue(QStringLiteral(":session_id"), session.id);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QVERIFY(documentValue(session.id, QStringLiteral("content")).toString().isEmpty());
}

void SessionRepositoryTest::update_leavesDocumentTimestampUnchangedForUnrelatedEdit()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    session.notes = QStringLiteral("stable note");
    QVERIFY(repository_->insert(session));

    const QString originalTimestamp = documentValue(session.id, QStringLiteral("last_saved_timestamp_utc")).toString();
    QVERIFY(!originalTimestamp.isEmpty());

    session.trackedDuration = seconds{999};
    QVERIFY(repository_->update(session));

    QCOMPARE(documentValue(session.id, QStringLiteral("last_saved_timestamp_utc")).toString(), originalTimestamp);
    QCOMPARE(repository_->query({})[0].trackedDuration, seconds{999});
}

void SessionRepositoryTest::update_assignsStrictlyLaterDocumentTimestampForChangedNote()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    session.notes = QStringLiteral("first");
    QVERIFY(repository_->insert(session));

    const QDateTime originalTimestamp = QDateTime::fromString(documentValue(session.id,
                                                                            QStringLiteral("last_saved_timestamp_utc")).
                                                              toString(),
                                                              Qt::ISODateWithMs);
    QVERIFY(originalTimestamp.isValid());

    session.notes = QStringLiteral("second");
    QVERIFY(repository_->update(session));

    const QDateTime updatedTimestamp = QDateTime::fromString(documentValue(session.id,
                                                                           QStringLiteral("last_saved_timestamp_utc")).
                                                             toString(),
                                                             Qt::ISODateWithMs);
    QVERIFY(updatedTimestamp.isValid());
    QVERIFY(updatedTimestamp > originalTimestamp);
    QCOMPARE(documentValue(session.id, QStringLiteral("content")).toString(), QStringLiteral("second"));
}

void SessionRepositoryTest::insert_rollsBackAndRestoresZeroIdWhenDocumentInsertFails()
{
    QSqlQuery trigger{manager_->database()};
    QVERIFY(trigger.exec(QStringLiteral("CREATE TRIGGER reject_document_insert BEFORE INSERT ON session_documents "
                                        "BEGIN SELECT RAISE(ABORT, 'document insert rejected'); END")));

    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Failed to insert session notes.*"));
    QVERIFY(!repository_->insert(session));

    // The caller's Session must not keep an ID that no longer exists.
    QCOMPARE(session.id, 0);
    QCOMPARE(sessionRowCount(), 0);
}

void SessionRepositoryTest::update_rollsBackSessionRowWhenDocumentUpdateFails()
{
    Session session = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    session.notes = QStringLiteral("original");
    QVERIFY(repository_->insert(session));

    QSqlQuery trigger{manager_->database()};
    QVERIFY(trigger.exec(QStringLiteral("CREATE TRIGGER reject_document_update BEFORE UPDATE ON session_documents "
                                        "BEGIN SELECT RAISE(ABORT, 'document update rejected'); END")));

    Session modified = session;
    modified.notes = QStringLiteral("changed");
    modified.trackedDuration = seconds{5555};

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Failed to update session notes.*"));
    QVERIFY(!repository_->update(modified));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].trackedDuration, seconds{120});
    QCOMPARE(sessions[0].notes, QStringLiteral("original"));
}

void SessionRepositoryTest::query_skipsRowWithUnknownSource()
{
    Session valid = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(valid));

    QVERIFY(insertRawSession(gameId_,
                             QStringLiteral("2026-03-02T12:00:00.000Z"),
                             QVariant{QStringLiteral("2026-03-02T13:00:00.000Z")},
                             3600,
                             QStringLiteral("telepathic"),
                             QStringLiteral("completed"),
                             true) > 0);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*invalid source or status.*"));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].id, valid.id);
}

void SessionRepositoryTest::query_skipsRowWithUnknownStatus()
{
    Session valid = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(valid));

    QVERIFY(insertRawSession(gameId_,
                             QStringLiteral("2026-03-02T12:00:00.000Z"),
                             QVariant{QStringLiteral("2026-03-02T13:00:00.000Z")},
                             3600,
                             QStringLiteral("automatic"),
                             QStringLiteral("hibernating"),
                             true) > 0);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*invalid source or status.*"));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].id, valid.id);
}

void SessionRepositoryTest::query_skipsRowWithNegativeTrackedDuration()
{
    Session valid = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(valid));

    QVERIFY(insertRawSession(gameId_,
                             QStringLiteral("2026-03-02T12:00:00.000Z"),
                             QVariant{QStringLiteral("2026-03-02T13:00:00.000Z")},
                             -60,
                             QStringLiteral("automatic"),
                             QStringLiteral("completed"),
                             true) > 0);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*tracked duration is negative.*"));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].id, valid.id);
}

void SessionRepositoryTest::query_skipsRowWithInvalidStartTimestamp()
{
    Session valid = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(valid));

    // No CHECK constraint governs timestamp text, so this row inserts directly.
    QVERIFY(insertRawSession(gameId_,
                             QStringLiteral("last tuesday, probably"),
                             QVariant{QStringLiteral("2026-03-02T13:00:00.000Z")},
                             3600,
                             QStringLiteral("automatic"),
                             QStringLiteral("completed")) > 0);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*start timestamp is invalid.*"));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].id, valid.id);
}

void SessionRepositoryTest::query_skipsActiveRowThatHasAnEndTimestamp()
{
    Session valid = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(valid));

    QVERIFY(insertRawSession(gameId_,
                             QStringLiteral("2026-03-02T12:00:00.000Z"),
                             QVariant{QStringLiteral("2026-03-02T13:00:00.000Z")},
                             3600,
                             QStringLiteral("automatic"),
                             QStringLiteral("active")) > 0);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*active session has an end timestamp.*"));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].id, valid.id);
}

void SessionRepositoryTest::query_skipsInactiveRowWhoseEndPrecedesStart()
{
    Session valid = makeSession(gameId_, utcDateTime(2026, 3, 1, 12, 0, 0), SessionStatus::Completed);
    QVERIFY(repository_->insert(valid));

    QVERIFY(insertRawSession(gameId_,
                             QStringLiteral("2026-03-02T12:00:00.000Z"),
                             QVariant{QStringLiteral("2026-03-02T11:00:00.000Z")},
                             3600,
                             QStringLiteral("automatic"),
                             QStringLiteral("interrupted")) > 0);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*end timestamp precedes start timestamp.*"));

    const std::vector<Session> sessions = repository_->query({});
    QCOMPARE(static_cast<int>(sessions.size()), 1);
    QCOMPARE(sessions[0].id, valid.id);
}

QTEST_GUILESS_MAIN(SessionRepositoryTest)

#include "SessionRepositoryTest.moc"
