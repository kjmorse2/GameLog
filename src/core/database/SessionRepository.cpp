#include "database/SessionRepository.h"

#include "logging/LoggingCategories.h"

#include <optional>

#include <QDateTime>
#include <QList>
#include <QPair>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

using gamelog::core::domain::SessionSource;
using gamelog::core::domain::SessionStatus;
using gamelog::core::domain::query::SessionQuery;
using gamelog::core::domain::query::SessionSortField;
using gamelog::core::domain::query::SortDirection;

namespace gamelog::core::database {
namespace {


QString sourceToString(SessionSource source)
{
    switch (source)
    {
        case SessionSource::Automatic:
            return QStringLiteral("automatic");
        case SessionSource::Manual:
            return QStringLiteral("manual");
    }
    return QStringLiteral("automatic");
}

QString statusToString(SessionStatus status)
{
    switch (status)
    {
        case SessionStatus::Active:
            return QStringLiteral("active");
        case SessionStatus::Completed:
            return QStringLiteral("completed");
        case SessionStatus::Interrupted:
            return QStringLiteral("interrupted");
    }
    return QStringLiteral("interrupted");
}

std::optional<SessionSource> sourceFromString(const QString &value)
{
    if (value == QStringLiteral("automatic"))
    {
        return SessionSource::Automatic;
    }
    if (value == QStringLiteral("manual"))
    {
        return SessionSource::Manual;
    }
    return std::nullopt;
}

std::optional<SessionStatus> statusFromString(const QString &value)
{
    if (value == QStringLiteral("active"))
    {
        return SessionStatus::Active;
    }
    if (value == QStringLiteral("completed"))
    {
        return SessionStatus::Completed;
    }
    if (value == QStringLiteral("interrupted"))
    {
        return SessionStatus::Interrupted;
    }
    return std::nullopt;
}

QDateTime dateTimeFromDatabase(const QVariant &value)
{
    QDateTime result = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
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

std::optional<Session> sessionFromQuery(const QSqlQuery &query)
{
    const auto source =
        sourceFromString(query.value(QStringLiteral("source")).toString());
    const auto status =
        statusFromString(query.value(QStringLiteral("status")).toString());

    if (!source || !status)
    {
        qCWarning(gamelogDatabaseLog)
            << "Session row contains an invalid source or status.";
        return std::nullopt;
    }

    Session session;
    session.id = query.value(QStringLiteral("id")).toInt();
    session.gameId = query.value(QStringLiteral("game_id")).toInt();
    session.startTimestamp = dateTimeFromDatabase(
        query.value(QStringLiteral("start_timestamp_utc")));

    const QVariant endTimestamp =
        query.value(QStringLiteral("end_timestamp_utc"));
    if (!endTimestamp.isNull())
    {
        session.endTimestamp = dateTimeFromDatabase(endTimestamp);
    }

    using DurationRep = std::chrono::seconds::rep;
    session.trackedDuration = std::chrono::seconds{static_cast<DurationRep>(
        query.value(QStringLiteral("tracked_duration_seconds")).toLongLong())};
    session.source = *source;
    session.status = *status;
    return session;
}

void bindEndTimestamp(
    QSqlQuery &query,
    const std::optional<QDateTime> &endTimestamp)
{
    query.bindValue(
        QStringLiteral(":end_timestamp_utc"),
        endTimestamp ? QVariant{dateTimeToDatabase(*endTimestamp)} : QVariant{});
}

QString orderColumn(SessionSortField field)
{
    switch (field)
    {
        case SessionSortField::StartTimestamp:
            return QStringLiteral("start_timestamp_utc");
        case SessionSortField::TrackedDuration:
            return QStringLiteral("tracked_duration_seconds");
        case SessionSortField::Id:
            return QStringLiteral("id");
    }
    return QStringLiteral("start_timestamp_utc");
}

void appendInPredicate(
    const QString &column,
    const QString &placeholderPrefix,
    const QList<QVariant> &values,
    QStringList &predicates,
    QList<QPair<QString, QVariant>> &bindings)
{
    if (values.isEmpty())
    {
        return;
    }

    QStringList placeholders;
    placeholders.reserve(values.size());
    for (qsizetype index = 0; index < values.size(); ++index)
    {
        const QString placeholder =
            QStringLiteral(":%1_%2").arg(placeholderPrefix).arg(index);
        placeholders.push_back(placeholder);
        bindings.push_back({placeholder, values.at(index)});
    }

    predicates.push_back(
        QStringLiteral("%1 IN (%2)")
            .arg(column, placeholders.join(QStringLiteral(", "))));
}

} // namespace

SessionRepository::SessionRepository(const QSqlDatabase &database) :
    database_{database}
{}

vector<Session> SessionRepository::query(const SessionQuery &specification) const
{
    QString sql = QStringLiteral(
        "SELECT id, game_id, start_timestamp_utc, end_timestamp_utc, "
        "tracked_duration_seconds, source, status FROM sessions");

    QStringList predicates;
    QList<QPair<QString, QVariant>> bindings;

    QList<QVariant> ids;
    for (const int id : specification.ids)
    {
        ids.push_back(id);
    }
    appendInPredicate(
        QStringLiteral("id"), QStringLiteral("session_id"), ids,
        predicates, bindings);

    QList<QVariant> gameIds;
    for (const int gameId : specification.gameIds)
    {
        gameIds.push_back(gameId);
    }
    appendInPredicate(
        QStringLiteral("game_id"), QStringLiteral("game_id"), gameIds,
        predicates, bindings);

    QList<QVariant> sources;
    for (const SessionSource source : specification.sources)
    {
        sources.push_back(sourceToString(source));
    }
    appendInPredicate(
        QStringLiteral("source"), QStringLiteral("source"), sources,
        predicates, bindings);

    QList<QVariant> statuses;
    for (const SessionStatus status : specification.statuses)
    {
        statuses.push_back(statusToString(status));
    }
    appendInPredicate(
        QStringLiteral("status"), QStringLiteral("status"), statuses,
        predicates, bindings);

    const auto addComparison = [&predicates, &bindings](
                                   const QString &column,
                                   const QString &comparison,
                                   const QString &placeholder,
                                   const QVariant &value) {
        predicates.push_back(
            column + QLatin1Char(' ') + comparison + QLatin1Char(' ') + placeholder);
        bindings.push_back({placeholder, value});
    };

    if (specification.startedAtOrAfter)
    {
        addComparison(
            QStringLiteral("start_timestamp_utc"), QStringLiteral(">="),
            QStringLiteral(":started_at_or_after"),
            dateTimeToDatabase(*specification.startedAtOrAfter));
    }

    if (specification.startedBefore)
    {
        addComparison(
            QStringLiteral("start_timestamp_utc"), QStringLiteral("<"),
            QStringLiteral(":started_before"),
            dateTimeToDatabase(*specification.startedBefore));
    }

    if (specification.minimumTrackedDuration)
    {
        addComparison(
            QStringLiteral("tracked_duration_seconds"), QStringLiteral(">="),
            QStringLiteral(":minimum_duration"),
            QVariant::fromValue<qlonglong>(
                specification.minimumTrackedDuration->count()));
    }

    if (specification.maximumTrackedDuration)
    {
        addComparison(
            QStringLiteral("tracked_duration_seconds"), QStringLiteral("<="),
            QStringLiteral(":maximum_duration"),
            QVariant::fromValue<qlonglong>(
                specification.maximumTrackedDuration->count()));
    }

    if (specification.hasEndTimestamp)
    {
        predicates.push_back(*specification.hasEndTimestamp
            ? QStringLiteral("end_timestamp_utc IS NOT NULL")
            : QStringLiteral("end_timestamp_utc IS NULL"));
    }

    if (!predicates.isEmpty())
    {
        sql += QStringLiteral(" WHERE ") + predicates.join(QStringLiteral(" AND "));
    }

    sql += QStringLiteral(" ORDER BY ") + orderColumn(specification.sortBy);
    sql += specification.sortDirection == SortDirection::Ascending
        ? QStringLiteral(" ASC")
        : QStringLiteral(" DESC");

    if (specification.limit)
    {
        sql += QStringLiteral(" LIMIT :limit");
        bindings.push_back({
            QStringLiteral(":limit"),
            QVariant::fromValue<qulonglong>(*specification.limit)});
    }

    if (specification.offset)
    {
        if (!specification.limit)
        {
            sql += QStringLiteral(" LIMIT -1");
        }
        sql += QStringLiteral(" OFFSET :offset");
        bindings.push_back({
            QStringLiteral(":offset"),
            QVariant::fromValue<qulonglong>(*specification.offset)});
    }

    QSqlQuery sqlQuery{database_};
    if (!sqlQuery.prepare(sql))
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to prepare session query:" << sqlQuery.lastError().text();
        return {};
    }

    for (const auto &[placeholder, value] : bindings)
    {
        sqlQuery.bindValue(placeholder, value);
    }

    if (!sqlQuery.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to execute session query:" << sqlQuery.lastError().text();
        return {};
    }

    vector<Session> sessions;
    while (sqlQuery.next())
    {
        if (const auto session = sessionFromQuery(sqlQuery))
        {
            sessions.push_back(*session);
        }
    }
    return sessions;
}

bool SessionRepository::insert(Session &session)
{
    if (session.id != 0)
    {
        qCWarning(gamelogDatabaseLog)
            << "Refusing to insert a session that already has an ID:" << session.id;
        return false;
    }

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "INSERT INTO sessions (game_id, start_timestamp_utc, end_timestamp_utc, "
        "tracked_duration_seconds, source, status) VALUES (:game_id, "
        ":start_timestamp_utc, :end_timestamp_utc, :tracked_duration_seconds, "
        ":source, :status)"));

    query.bindValue(QStringLiteral(":game_id"), session.gameId);
    query.bindValue(
        QStringLiteral(":start_timestamp_utc"),
        dateTimeToDatabase(session.startTimestamp));
    bindEndTimestamp(query, session.endTimestamp);
    query.bindValue(
        QStringLiteral(":tracked_duration_seconds"),
        QVariant::fromValue<qlonglong>(session.trackedDuration.count()));
    query.bindValue(QStringLiteral(":source"), sourceToString(session.source));
    query.bindValue(QStringLiteral(":status"), statusToString(session.status));

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to insert session:" << query.lastError().text();
        return false;
    }

    const QVariant insertedId = query.lastInsertId();
    if (!insertedId.isValid() || insertedId.toLongLong() <= 0)
    {
        qCWarning(gamelogDatabaseLog)
            << "Session insert returned no valid primary key.";
        return false;
    }

    session.id = insertedId.toInt();
    return true;
}

bool SessionRepository::update(const Session &session)
{
    if (session.id <= 0)
    {
        return false;
    }

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE sessions SET game_id = :game_id, "
        "start_timestamp_utc = :start_timestamp_utc, "
        "end_timestamp_utc = :end_timestamp_utc, "
        "tracked_duration_seconds = :tracked_duration_seconds, source = :source, "
        "status = :status WHERE id = :id"));

    query.bindValue(QStringLiteral(":game_id"), session.gameId);
    query.bindValue(
        QStringLiteral(":start_timestamp_utc"),
        dateTimeToDatabase(session.startTimestamp));
    bindEndTimestamp(query, session.endTimestamp);
    query.bindValue(
        QStringLiteral(":tracked_duration_seconds"),
        QVariant::fromValue<qlonglong>(session.trackedDuration.count()));
    query.bindValue(QStringLiteral(":source"), sourceToString(session.source));
    query.bindValue(QStringLiteral(":status"), statusToString(session.status));
    query.bindValue(QStringLiteral(":id"), session.id);

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to update session:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() == 1;
}

bool SessionRepository::remove(int sessionId)
{
    if (sessionId <= 0)
    {
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
    return query.numRowsAffected() == 1;
}

} // namespace gamelog::core::database
