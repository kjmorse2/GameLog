#include "database/SessionRepository.h"

#include "logging/LoggingCategories.h"

#include <chrono>
#include <optional>
#include <utility>

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace gamelog::core::database {
    namespace {

        QString sourceToString(domain::SessionSource source)
        {
            switch (source)
            {
                case domain::SessionSource::Automatic:
                    return QStringLiteral("automatic");
                case domain::SessionSource::Manual:
                    return QStringLiteral("manual");
            }

            return QStringLiteral("automatic");
        }

        QString statusToString(domain::SessionStatus status)
        {
            switch (status)
            {
                case domain::SessionStatus::Active:
                    return QStringLiteral("active");
                case domain::SessionStatus::Completed:
                    return QStringLiteral("completed");
                case domain::SessionStatus::Interrupted:
                    return QStringLiteral("interrupted");
            }

            return QStringLiteral("interrupted");
        }

        std::optional<domain::SessionSource>
        sourceFromString(const QString &value)
        {
            if (value == QStringLiteral("automatic"))
            {
                return domain::SessionSource::Automatic;
            }

            if (value == QStringLiteral("manual"))
            {
                return domain::SessionSource::Manual;
            }

            return std::nullopt;
        }

        std::optional<domain::SessionStatus>
        statusFromString(const QString &value)
        {
            if (value == QStringLiteral("active"))
            {
                return domain::SessionStatus::Active;
            }

            if (value == QStringLiteral("completed"))
            {
                return domain::SessionStatus::Completed;
            }

            if (value == QStringLiteral("interrupted"))
            {
                return domain::SessionStatus::Interrupted;
            }

            return std::nullopt;
        }

        QDateTime dateTimeFromDatabase(const QVariant &value)
        {
            QDateTime result =
                    QDateTime::fromString(value.toString(), Qt::ISODateWithMs);

            if (!result.isValid())
            {
                result = QDateTime::fromString(value.toString(), Qt::ISODate);
            }

            return result;
        }

        QString dateTimeToDatabase(const QDateTime &value)
        {
            return value.toUTC().toString(Qt::ISODateWithMs);
        }

        std::optional<domain::Session>
        sessionFromQuery(const QSqlQuery &query)
        {
            const auto source = sourceFromString(query.value(QStringLiteral("source")).toString());
            const auto status = statusFromString(query.value(QStringLiteral("status")).toString());

            if (!source || !status)
            {
                qCWarning(gamelogDatabaseLog) << "Session row contains an invalid source or status.";
                return std::nullopt;
            }

            domain::Session session;
            session.id = query.value(QStringLiteral("id")).toInt();
            session.gameId = query.value(QStringLiteral("game_id")).toInt();
            session.startTimestamp = dateTimeFromDatabase(query.value(QStringLiteral("start_timestamp_utc")));

            const QVariant endTimestamp = query.value(QStringLiteral("end_timestamp_utc"));

            if (!endTimestamp.isNull())
            {
                session.endTimestamp = dateTimeFromDatabase(endTimestamp);
            }

            session.trackedDuration = std::chrono::seconds{
                    static_cast<std::chrono::seconds::rep>(
                            query.value(QStringLiteral("tracked_duration_seconds")).toLongLong())};
            session.source = *source;
            session.status = *status;
            return session;
        }

        void bindEndTimestamp(QSqlQuery &query, const std::optional<QDateTime> &endTimestamp)
        {
            if (endTimestamp)
            {
                query.bindValue(QStringLiteral(":end_timestamp_utc"), dateTimeToDatabase(*endTimestamp));
            }
            else
            {
                query.bindValue(QStringLiteral(":end_timestamp_utc"), QVariant{});
            }
        }

    } // namespace

    SessionRepository::SessionRepository(const QSqlDatabase &database) :
        database_{database}
    {}

    std::optional<domain::Session>
    SessionRepository::findActiveSession() const
    {
        QSqlQuery query{database_};

        if (!query.exec(
                    R"(
                    SELECT
                        id,
                        game_id,
                        start_timestamp_utc,
                        end_timestamp_utc,
                        tracked_duration_seconds,
                        source,
                        status
                    FROM sessions
                    WHERE status = 'active'
                    LIMIT 1
                )"))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to find active session:" << query.lastError().text();
            return std::nullopt;
        }

        if (!query.next())
        {
            return std::nullopt;
        }

        return sessionFromQuery(query);
    }

    std::vector<domain::Session>
    SessionRepository::listSessionsForGame(int gameId) const
    {
        QSqlQuery query{database_};
        query.prepare(
                R"(
                SELECT
                    id,
                    game_id,
                    start_timestamp_utc,
                    end_timestamp_utc,
                    tracked_duration_seconds,
                    source,
                    status
                FROM sessions
                WHERE game_id = :game_id
                ORDER BY start_timestamp_utc DESC
            )");
        query.bindValue(QStringLiteral(":game_id"), gameId);

        if (!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to list sessions for game:" << query.lastError().text();
            return {};
        }

        std::vector<domain::Session> sessions;

        while (query.next())
        {
            if (const auto session = sessionFromQuery(query); session)
            {
                sessions.push_back(*session);
            }
        }

        return sessions;
    }

    bool SessionRepository::insert(domain::Session &session)
    {
        if (session.id != 0)
        {
            qCWarning(gamelogDatabaseLog) << "Refusing to insert a session that already has an ID:" << session.id;
            return false;
        }

        QSqlQuery query{database_};
        query.prepare(
                R"(
                INSERT INTO sessions
                (
                    game_id,
                    start_timestamp_utc,
                    end_timestamp_utc,
                    tracked_duration_seconds,
                    source,
                    status
                )
                VALUES
                (
                    :game_id,
                    :start_timestamp_utc,
                    :end_timestamp_utc,
                    :tracked_duration_seconds,
                    :source,
                    :status
                )
            )");

        query.bindValue(QStringLiteral(":game_id"), session.gameId);
        query.bindValue(QStringLiteral(":start_timestamp_utc"), dateTimeToDatabase(session.startTimestamp));
        bindEndTimestamp(query, session.endTimestamp);
        query.bindValue(QStringLiteral(":tracked_duration_seconds"), QVariant::fromValue<qlonglong>(session.trackedDuration.count()));
        query.bindValue(QStringLiteral(":source"), sourceToString(session.source));
        query.bindValue(QStringLiteral(":status"), statusToString(session.status));

        if (!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to insert session:" << query.lastError().text();
            return false;
        }

        const QVariant insertedId = query.lastInsertId();

        if (!insertedId.isValid() || insertedId.toLongLong() <= 0)
        {
            qCWarning(gamelogDatabaseLog) << "Session insert succeeded but returned no valid primary key.";
            return false;
        }

        session.id = insertedId.toInt();
        return true;
    }

    bool SessionRepository::update(const domain::Session &session)
    {
        if (session.id <= 0)
        {
            qCWarning(gamelogDatabaseLog) << "Refusing to update a session with an invalid ID:" << session.id;
            return false;
        }

        QSqlQuery query{database_};
        query.prepare(
                R"(
                UPDATE sessions
                SET
                    game_id = :game_id,
                    start_timestamp_utc = :start_timestamp_utc,
                    end_timestamp_utc = :end_timestamp_utc,
                    tracked_duration_seconds = :tracked_duration_seconds,
                    source = :source,
                    status = :status
                WHERE id = :id
            )");

        query.bindValue(QStringLiteral(":game_id"), session.gameId);
        query.bindValue(QStringLiteral(":start_timestamp_utc"), dateTimeToDatabase(session.startTimestamp));
        bindEndTimestamp(query, session.endTimestamp);
        query.bindValue(QStringLiteral(":tracked_duration_seconds"), QVariant::fromValue<qlonglong>(session.trackedDuration.count()));
        query.bindValue(QStringLiteral(":source"), sourceToString(session.source));
        query.bindValue(QStringLiteral(":status"), statusToString(session.status));
        query.bindValue(QStringLiteral(":id"), session.id);

        if (!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to update session:" << query.lastError().text();
            return false;
        }

        if (query.numRowsAffected() != 1)
        {
            qCWarning(gamelogDatabaseLog) << "Session update expected one row but changed" << query.numRowsAffected() << "for session ID" << session.id;
            return false;
        }

        return true;
    }

    bool SessionRepository::remove(int sessionId)
    {
        if (sessionId <= 0)
        {
            qCWarning(gamelogDatabaseLog) << "Refusing to delete a session with an invalid ID:" << sessionId;
            return false;
        }

        QSqlQuery query{database_};
        query.prepare(QStringLiteral("DELETE FROM sessions WHERE id = :id"));
        query.bindValue(QStringLiteral(":id"), sessionId);

        if (!query.exec())
        {
            qCWarning(gamelogDatabaseLog)
                    << "Failed to delete session:" << query.lastError().text();
            return false;
        }

        if (query.numRowsAffected() != 1)
        {
            qCWarning(gamelogDatabaseLog) << "Session delete expected one row but changed" << query.numRowsAffected() << "for session ID" << sessionId;
            return false;
        }

        return true;
    }

} // namespace gamelog::core::database
