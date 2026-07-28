#include "database/SessionRepository.h"

#include "logging/LoggingCategories.h"

#include <chrono>
#include <optional>
#include <utility>

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace gamelog::core::database
{
namespace
{

QString sourceToString(domain::SessionSource source)
{
    switch (source)
    {
    case domain::SessionSource::Automatic:
        return "automatic";
    case domain::SessionSource::Manual:
        return "manual";
    }

    return "automatic";
}

QString statusToString(domain::SessionStatus status)
{
    switch (status)
    {
    case domain::SessionStatus::Active:
        return "active";
    case domain::SessionStatus::Completed:
        return "completed";
    case domain::SessionStatus::Interrupted:
        return "interrupted";
    }

    return "interrupted";
}

std::optional<domain::SessionSource>
sourceFromString(const QString& value)
{
    if (value == "automatic")
    {
        return domain::SessionSource::Automatic;
    }

    if (value == "manual")
    {
        return domain::SessionSource::Manual;
    }

    return std::nullopt;
}

std::optional<domain::SessionStatus>
statusFromString(const QString& value)
{
    if (value == "active")
    {
        return domain::SessionStatus::Active;
    }

    if (value == "completed")
    {
        return domain::SessionStatus::Completed;
    }

    if (value == "interrupted")
    {
        return domain::SessionStatus::Interrupted;
    }

    return std::nullopt;
}

QDateTime dateTimeFromDatabase(
    const QVariant& value
)
{
    QDateTime result = QDateTime::fromString(
        value.toString(),
        Qt::ISODateWithMs
    );

    if (!result.isValid())
    {
        result = QDateTime::fromString(
            value.toString(),
            Qt::ISODate
        );
    }

    return result;
}

QString dateTimeToDatabase(
    const QDateTime& value
)
{
    return value.toUTC().toString(Qt::ISODateWithMs);
}

std::optional<domain::Session>
sessionFromQuery(const QSqlQuery& query)
{
    const std::optional<domain::SessionSource> source =
        sourceFromString(
            query.value("source").toString()
        );
    const std::optional<domain::SessionStatus> status =
        statusFromString(
            query.value("status").toString()
        );

    if (!source.has_value() || !status.has_value())
    {
        qCWarning(gamelogDatabaseLog)
            << "Session row contains an invalid source or status.";
        return std::nullopt;
    }

    domain::Session session;
    session.id = query.value("id").toInt();
    session.gameId = query.value("game_id").toInt();
    session.startTimestamp = dateTimeFromDatabase(
        query.value("start_timestamp_utc")
    );

    const QVariant endTimestamp =
        query.value("end_timestamp_utc");
    if (!endTimestamp.isNull())
    {
        session.endTimestamp =
            dateTimeFromDatabase(endTimestamp);
    }

    session.trackedDuration = std::chrono::seconds{
        static_cast<std::chrono::seconds::rep>(
            query.value(
                "tracked_duration_seconds"
            ).toLongLong()
        )
    };
    session.source = *source;
    session.status = *status;

    return session;
}

void bindEndTimestamp(QSqlQuery& query, const std::optional<QDateTime>& endTimestamp)
{
    if (endTimestamp.has_value())
    {
        query.bindValue(
            ":end_timestamp_utc",
            dateTimeToDatabase(*endTimestamp)
        );
    }
    else
    {
        query.bindValue(
            ":end_timestamp_utc",
            QVariant{}
        );
    }
}

} // namespace

SessionRepository::SessionRepository(QSqlDatabase database)
    : database_{std::move(database)}
{}

std::optional<domain::Session> SessionRepository::findActiveSession() const
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
        qCWarning(gamelogDatabaseLog)
            << "Failed to find active session:"
            << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
    {
        return std::nullopt;
    }

    return sessionFromQuery(query);
}

std::vector<domain::Session>
SessionRepository::listSessionsForGame(
    int gameId
) const
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
        )"
    );
    query.bindValue(":game_id", gameId);

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to list sessions for game:"
            << query.lastError().text();

        return {};
    }

    std::vector<domain::Session> sessions;

    while (query.next())
    {
        const auto session = sessionFromQuery(query);

        if (session.has_value())
        {
            sessions.push_back(*session);
        }
    }

    return sessions;
}

bool SessionRepository::insert(domain::Session& session)
{
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
        )"
    );

    query.bindValue(":game_id", session.gameId);
    query.bindValue(":start_timestamp_utc", dateTimeToDatabase(session.startTimestamp));
    bindEndTimestamp(query, session.endTimestamp);
    query.bindValue(
        ":tracked_duration_seconds",
        QVariant::fromValue<qlonglong>(
            static_cast<qlonglong>(
                session.trackedDuration.count()
            )
        )
    );
    query.bindValue(
        ":source",
        sourceToString(session.source)
    );
    query.bindValue(
        ":status",
        statusToString(session.status)
    );

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to insert session:"
            << query.lastError().text();

        return false;
    }

    session.id = query.lastInsertId().toInt();
    return true;
}

bool SessionRepository::update(const domain::Session& session)
{
    QSqlQuery query{database_};
    query.prepare(
        R"(
            UPDATE sessions
            SET
                game_id = :game_id,
                start_timestamp_utc =
                    :start_timestamp_utc,
                end_timestamp_utc =
                    :end_timestamp_utc,
                tracked_duration_seconds =
                    :tracked_duration_seconds,
                source = :source,
                status = :status
            WHERE id = :id
        )"
    );

    query.bindValue(":game_id", session.gameId);
    query.bindValue(
        ":start_timestamp_utc",
        dateTimeToDatabase(session.startTimestamp)
    );
    bindEndTimestamp(query, session.endTimestamp);
    query.bindValue(
        ":tracked_duration_seconds",
        QVariant::fromValue<qlonglong>(
            static_cast<qlonglong>(
                session.trackedDuration.count()
            )
        )
    );
    query.bindValue(
        ":source",
        sourceToString(session.source)
    );
    query.bindValue(
        ":status",
        statusToString(session.status)
    );
    query.bindValue(":id", session.id);

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to update session:"
            << query.lastError().text();

        return false;
    }

    return true;
}

bool SessionRepository::remove(int sessionId)
{
    QSqlQuery query{database_};
    query.prepare(
        "DELETE FROM sessions WHERE id = :id"
    );
    query.bindValue(":id", sessionId);

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to delete session:"
            << query.lastError().text();

        return false;
    }

    return true;
}
} // namespace gamelog::core::database
