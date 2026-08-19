#include "database/SessionRepository.h"
#include "database/SqlQueryBuilder.h"

#include "logging/LoggingCategories.h"

#include <chrono>
#include <optional>

#include <QDateTime>
#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

using gamelog::core::domain::SessionSource;
using gamelog::core::domain::SessionStatus;
using gamelog::core::domain::query::SessionQuery;
using gamelog::core::domain::query::SessionSortField;
using gamelog::core::domain::query::SortDirection;

namespace gamelog::core::database
{
    using domain::Session;

    namespace
    {
        QDateTime dateTimeFromDatabase(const QVariant& value)
        {
            QDateTime result = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
            if(!result.isValid()) { result = QDateTime::fromString(value.toString(), Qt::ISODate); }
            return result;
        }

        QString dateTimeToDatabase(const QDateTime& value) { return value.toUTC().toString(Qt::ISODateWithMs); }

        // session_documents.content is NOT NULL, but a default-constructed or
        // cleared QString is null rather than empty and would bind as SQL NULL.
        // Empty notes must still produce a document row, so normalize first.
        QString notesToDatabase(const QString& notes) { return notes.isNull() ? QStringLiteral("") : notes; }

        // Persisted timestamps may lack milliseconds (legacy rows) or use a
        // trailing 'Z' inconsistently with the bound parameter's literal text,
        // so raw TEXT comparison cannot be trusted for range predicates.
        // strftime() normalizes both the column and the bound value to the
        // same millisecond-precision, offset-free form before comparing, and
        // returns NULL for unparseable text so corrupted rows are excluded by
        // SQL's three-valued logic rather than reaching sessionFromQuery().
        QString normalizedTimestampExpression(const QString& sqlExpression)
        {
            return QStringLiteral("strftime('%Y-%m-%dT%H:%M:%f', %1)").arg(sqlExpression);
        }

        bool validateSession(const Session& session, QString* reason = nullptr)
        {
            const auto fail = [reason](const QString& message)
            {
                if(reason != nullptr) { *reason = message; }
                return false;
            };

            switch(session.source)
            {
            case SessionSource::Automatic:
            case SessionSource::Manual:
                break;
            default:
                return fail(QStringLiteral("session source is invalid"));
            }

            switch(session.status)
            {
            case SessionStatus::Active:
            case SessionStatus::Completed:
            case SessionStatus::Interrupted:
                break;
            default:
                return fail(QStringLiteral("session status is invalid"));
            }

            if(!session.startTimestamp.isValid()) { return fail(QStringLiteral("start timestamp is invalid")); }

            if(session.trackedDuration < std::chrono::seconds::zero())
            {
                return fail(QStringLiteral("tracked duration is negative"));
            }

            if(session.status == SessionStatus::Active)
            {
                if(session.endTimestamp.has_value())
                {
                    return fail(QStringLiteral("active session has an end timestamp"));
                }
                return true;
            }

            if(!session.endTimestamp || !session.endTimestamp->isValid())
            {
                return fail(QStringLiteral("completed or interrupted session lacks a valid end timestamp"));
            }

            if(*session.endTimestamp < session.startTimestamp)
            {
                return fail(QStringLiteral("end timestamp precedes start timestamp"));
            }

            return true;
        }

        std::optional<Session> sessionFromQuery(const QSqlQuery& query)
        {
            const auto source = domain::sessionSourceFromDatabase(query.value(QStringLiteral("source")).toString());
            const auto status = domain::sessionStatusFromDatabase(query.value(QStringLiteral("status")).toString());

            if(!source || !status)
            {
                qCWarning(gamelogDatabaseLog) << "Session row" << query.value(QStringLiteral("id")) <<
                    "contains an invalid source or status.";
                return std::nullopt;
            }

            Session session;
            session.id = query.value(QStringLiteral("id")).toInt();
            session.gameId = query.value(QStringLiteral("game_id")).toInt();
            session.startTimestamp = dateTimeFromDatabase(query.value(QStringLiteral("start_timestamp_utc")));

            const QVariant endTimestamp = query.value(QStringLiteral("end_timestamp_utc"));
            if(!endTimestamp.isNull()) { session.endTimestamp = dateTimeFromDatabase(endTimestamp); }

            using DurationRep = std::chrono::seconds::rep;
            session.trackedDuration = std::chrono::seconds{
                static_cast<DurationRep>(query.value(QStringLiteral("tracked_duration_seconds")).toLongLong())
            };
            session.source = *source;
            session.status = *status;
            session.notes = query.value(QStringLiteral("notes")).toString();

            QString validationError;
            if(!validateSession(session, &validationError))
            {
                qCWarning(gamelogDatabaseLog) << "Skipping corrupted session row" << session.id << ":" <<
                    validationError;
                return std::nullopt;
            }

            return session;
        }

        void bindEndTimestamp(QSqlQuery& query, const std::optional<QDateTime>& endTimestamp)
        {
            query.bindValue(QStringLiteral(":end_timestamp_utc"),
                            endTimestamp ? QVariant{dateTimeToDatabase(*endTimestamp)} : QVariant{});
        }

        bool insertSessionDocument(const QSqlDatabase& database, int sessionId, const QString& notes)
        {
            QSqlQuery notesQuery{database};
            notesQuery.
                prepare(QStringLiteral("INSERT INTO session_documents (session_id, content, last_saved_timestamp_utc) "
                                       "VALUES (:session_id, :content, :last_saved_timestamp_utc)"));
            notesQuery.bindValue(QStringLiteral(":session_id"), sessionId);
            notesQuery.bindValue(QStringLiteral(":content"), notesToDatabase(notes));
            notesQuery.bindValue(QStringLiteral(":last_saved_timestamp_utc"),
                                 dateTimeToDatabase(QDateTime::currentDateTimeUtc()));

            if(!notesQuery.exec())
            {
                qCWarning(gamelogDatabaseLog) << "Failed to insert session notes:" << notesQuery.lastError().text();
                return false;
            }

            return true;
        }

        bool saveSessionDocumentIfChanged(const QSqlDatabase& database, int sessionId, const QString& notes)
        {
            QSqlQuery existingQuery{database};
            existingQuery.prepare(QStringLiteral("SELECT content, last_saved_timestamp_utc "
                                                 "FROM session_documents WHERE session_id = :session_id"));
            existingQuery.bindValue(QStringLiteral(":session_id"), sessionId);

            if(!existingQuery.exec())
            {
                qCWarning(gamelogDatabaseLog) << "Failed to inspect existing session notes:" << existingQuery.
                    lastError().text();
                return false;
            }

            if(!existingQuery.next())
            {
                existingQuery.finish();
                return insertSessionDocument(database, sessionId, notes);
            }

            if(existingQuery.value(QStringLiteral("content")).toString() == notes)
            {
                existingQuery.finish();
                return true;
            }

            const QDateTime previousSavedAt =
                dateTimeFromDatabase(existingQuery.value(QStringLiteral("last_saved_timestamp_utc")));
            existingQuery.finish();

            QDateTime savedAt = QDateTime::currentDateTimeUtc();
            if(previousSavedAt.isValid() && savedAt <= previousSavedAt) { savedAt = previousSavedAt.addMSecs(1); }

            QSqlQuery updateQuery{database};
            updateQuery.prepare(QStringLiteral("UPDATE session_documents SET content = :content, "
                                               "last_saved_timestamp_utc = :last_saved_timestamp_utc "
                                               "WHERE session_id = :session_id"));
            updateQuery.bindValue(QStringLiteral(":content"), notesToDatabase(notes));
            updateQuery.bindValue(QStringLiteral(":last_saved_timestamp_utc"), dateTimeToDatabase(savedAt));
            updateQuery.bindValue(QStringLiteral(":session_id"), sessionId);

            if(!updateQuery.exec() || updateQuery.numRowsAffected() != 1)
            {
                qCWarning(gamelogDatabaseLog) << "Failed to update session notes:" << updateQuery.lastError().text();
                return false;
            }

            return true;
        }

        QString orderColumn(SessionSortField field)
        {
            switch(field)
            {
            case SessionSortField::StartTimestamp:
                return QStringLiteral("s.start_timestamp_utc");
            case SessionSortField::TrackedDuration:
                return QStringLiteral("s.tracked_duration_seconds");
            case SessionSortField::Id:
                return QStringLiteral("s.id");
            }

            return QStringLiteral("s.start_timestamp_utc");
        }
    } // namespace

    SessionRepository::SessionRepository(const QSqlDatabase& database) : database_{database} {}

    std::vector<domain::Session> SessionRepository::query(const domain::query::SessionQuery& specification) const
    {
        const QString baseSql = QStringLiteral("SELECT s.id AS id, s.game_id AS game_id, "
                                               "s.start_timestamp_utc AS start_timestamp_utc, "
                                               "s.end_timestamp_utc AS end_timestamp_utc, "
                                               "s.tracked_duration_seconds AS tracked_duration_seconds, "
                                               "s.source AS source, s.status AS status, "
                                               "COALESCE(d.content, '') AS notes " "FROM sessions AS s "
                                               "LEFT JOIN session_documents AS d ON d.session_id = s.id");

        SqlQueryBuilder builder;

        QList<QVariant> ids;
        for(const int id : specification.ids) { ids.push_back(id); }
        builder.addInPredicate(QStringLiteral("s.id"), QStringLiteral("session_id"), ids);

        QList<QVariant> gameIds;
        for(const int gameId : specification.gameIds) { gameIds.push_back(gameId); }
        builder.addInPredicate(QStringLiteral("s.game_id"), QStringLiteral("game_id"), gameIds);

        QList<QVariant> sources;
        for(const SessionSource source : specification.sources) { sources.push_back(domain::toDatabaseString(source)); }
        builder.addInPredicate(QStringLiteral("s.source"), QStringLiteral("source"), sources);

        QList<QVariant> statuses;
        for(const SessionStatus status : specification.statuses)
        {
            statuses.push_back(domain::toDatabaseString(status));
        }
        builder.addInPredicate(QStringLiteral("s.status"), QStringLiteral("status"), statuses);

        if(specification.startedAtOrAfter)
        {
            builder.addPredicate(normalizedTimestampExpression(QStringLiteral("s.start_timestamp_utc")) +
                                 QStringLiteral(" >= ") +
                                 normalizedTimestampExpression(QStringLiteral(":started_at_or_after")),
                                 QStringLiteral(":started_at_or_after"),
                                 dateTimeToDatabase(*specification.startedAtOrAfter));
        }

        if(specification.startedBefore)
        {
            builder.addPredicate(normalizedTimestampExpression(QStringLiteral("s.start_timestamp_utc")) +
                                 QStringLiteral(" < ") +
                                 normalizedTimestampExpression(QStringLiteral(":started_before")),
                                 QStringLiteral(":started_before"),
                                 dateTimeToDatabase(*specification.startedBefore));
        }

        if(specification.minimumTrackedDuration)
        {
            builder.addPredicate(QStringLiteral("s.tracked_duration_seconds >= :minimum_duration"),
                                 QStringLiteral(":minimum_duration"),
                                 QVariant::fromValue<qlonglong>(specification.minimumTrackedDuration->count()));
        }

        if(specification.maximumTrackedDuration)
        {
            builder.addPredicate(QStringLiteral("s.tracked_duration_seconds <= :maximum_duration"),
                                 QStringLiteral(":maximum_duration"),
                                 QVariant::fromValue<qlonglong>(specification.maximumTrackedDuration->count()));
        }

        if(specification.hasEndTimestamp)
        {
            builder.addPredicate(*specification.hasEndTimestamp
                                     ? QStringLiteral("s.end_timestamp_utc IS NOT NULL")
                                     : QStringLiteral("s.end_timestamp_utc IS NULL"));
        }

        builder.setOrderBy(orderColumn(specification.sortBy), specification.sortDirection);
        builder.setLimitOffset(specification.limit, specification.offset);

        const QString sql = builder.buildSql(baseSql);

        QSqlQuery sqlQuery{database_};
        if(!sqlQuery.prepare(sql))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to prepare session query:" << sqlQuery.lastError().text();
            return {};
        }

        builder.bindTo(sqlQuery);

        if(!sqlQuery.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to execute session query:" << sqlQuery.lastError().text();
            return {};
        }

        std::vector<domain::Session> sessions;
        while(sqlQuery.next()) { if(const auto session = sessionFromQuery(sqlQuery)) { sessions.push_back(*session); } }
        return sessions;
    }

    bool SessionRepository::insert(domain::Session& session)
    {
        if(session.id != 0)
        {
            qCWarning(gamelogDatabaseLog) << "Refusing to insert a session that already has an ID:" << session.id;
            return false;
        }

        QString validationError;
        if(!validateSession(session, &validationError))
        {
            qCWarning(gamelogDatabaseLog) << "Refusing to insert invalid session:" << validationError;
            return false;
        }

        if(!database_.transaction())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to begin session insert transaction:" << database_.lastError().
                text();
            return false;
        }

        QSqlQuery query{database_};
        query.prepare(QStringLiteral("INSERT INTO sessions (game_id, start_timestamp_utc, end_timestamp_utc, "
                                     "tracked_duration_seconds, source, status) VALUES (:game_id, "
                                     ":start_timestamp_utc, :end_timestamp_utc, :tracked_duration_seconds, :source, :status)"));
        query.bindValue(QStringLiteral(":game_id"), session.gameId);
        query.bindValue(QStringLiteral(":start_timestamp_utc"), dateTimeToDatabase(session.startTimestamp));
        bindEndTimestamp(query, session.endTimestamp);
        query.bindValue(QStringLiteral(":tracked_duration_seconds"),
                        QVariant::fromValue<qlonglong>(session.trackedDuration.count()));
        query.bindValue(QStringLiteral(":source"), domain::toDatabaseString(session.source));
        query.bindValue(QStringLiteral(":status"), domain::toDatabaseString(session.status));

        if(!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to insert session:" << query.lastError().text();
            database_.rollback();
            return false;
        }

        const QVariant insertedId = query.lastInsertId();
        if(!insertedId.isValid() || insertedId.toLongLong() <= 0)
        {
            qCWarning(gamelogDatabaseLog) << "Session insert returned no valid primary key.";
            database_.rollback();
            return false;
        }

        session.id = insertedId.toInt();

        if(!insertSessionDocument(database_, session.id, session.notes))
        {
            database_.rollback();
            session.id = 0;
            return false;
        }

        if(!database_.commit())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to commit session insert transaction:" << database_.lastError().
                text();
            database_.rollback();
            session.id = 0;
            return false;
        }

        return true;
    }

    bool SessionRepository::update(const domain::Session& session)
    {
        if(session.id <= 0) { return false; }

        QString validationError;
        if(!validateSession(session, &validationError))
        {
            qCWarning(gamelogDatabaseLog) << "Refusing to update invalid session" << session.id << ":" <<
                validationError;
            return false;
        }

        if(!database_.transaction())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to begin session update transaction:" << database_.lastError().
                text();
            return false;
        }

        QSqlQuery query{database_};
        query.prepare(QStringLiteral("UPDATE sessions SET game_id = :game_id, "
                                     "start_timestamp_utc = :start_timestamp_utc, "
                                     "end_timestamp_utc = :end_timestamp_utc, "
                                     "tracked_duration_seconds = :tracked_duration_seconds, "
                                     "source = :source, status = :status WHERE id = :id"));
        query.bindValue(QStringLiteral(":game_id"), session.gameId);
        query.bindValue(QStringLiteral(":start_timestamp_utc"), dateTimeToDatabase(session.startTimestamp));
        bindEndTimestamp(query, session.endTimestamp);
        query.bindValue(QStringLiteral(":tracked_duration_seconds"),
                        QVariant::fromValue<qlonglong>(session.trackedDuration.count()));
        query.bindValue(QStringLiteral(":source"), domain::toDatabaseString(session.source));
        query.bindValue(QStringLiteral(":status"), domain::toDatabaseString(session.status));
        query.bindValue(QStringLiteral(":id"), session.id);

        if(!query.exec() || query.numRowsAffected() != 1)
        {
            qCWarning(gamelogDatabaseLog) << "Failed to update session:" << query.lastError().text();
            database_.rollback();
            return false;
        }

        if(!saveSessionDocumentIfChanged(database_, session.id, session.notes))
        {
            database_.rollback();
            return false;
        }

        if(!database_.commit())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to commit session update transaction:" << database_.lastError().
                text();
            database_.rollback();
            return false;
        }

        return true;
    }

    bool SessionRepository::remove(int sessionId) const
    {
        if(sessionId <= 0) { return false; }

        QSqlQuery query{database_};
        query.prepare(QStringLiteral("DELETE FROM sessions WHERE id = :id"));
        query.bindValue(QStringLiteral(":id"), sessionId);

        if(!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to delete session:" << query.lastError().text();
            return false;
        }

        return query.numRowsAffected() == 1;
    }
} // namespace gamelog::core::database
