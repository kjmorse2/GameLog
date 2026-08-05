#include "database/GameRepository.h"

#include "logging/LoggingCategories.h"

#include <optional>

#include <QList>
#include <QPair>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace gamelog::core::database {
namespace {

using domain::query::GameQuery;
using domain::query::GameSortField;
using domain::query::SortDirection;

domain::Game gameFromQuery(const QSqlQuery &query)
{
    domain::Game game;
    game.id = query.value(QStringLiteral("id")).toInt();
    game.title = query.value(QStringLiteral("title")).toString();
    game.executablePath = query.value(QStringLiteral("executable_path")).toString();
    game.executableName = query.value(QStringLiteral("executable_name")).toString();

    const QVariant steamAppId = query.value(QStringLiteral("steam_app_id"));
    if (!steamAppId.isNull())
    {
        game.steamAppId = steamAppId.toInt();
    }

    const QVariant artworkPath = query.value(QStringLiteral("artwork_path"));
    if (!artworkPath.isNull())
    {
        game.artworkPath = artworkPath.toString();
    }

    game.trackingEnabled = query.value(QStringLiteral("tracking_enabled")).toBool();
    return game;
}

void bindNullableInt(
    QSqlQuery &query,
    const QString &placeholder,
    const std::optional<int> &value)
{
    query.bindValue(placeholder, value ? QVariant{*value} : QVariant{});
}

void bindNullableString(
    QSqlQuery &query,
    const QString &placeholder,
    const std::optional<QString> &value)
{
    query.bindValue(placeholder, value ? QVariant{*value} : QVariant{});
}

QString orderColumn(GameSortField field)
{
    switch (field)
    {
        case GameSortField::Title:
            return QStringLiteral("title COLLATE NOCASE");
        case GameSortField::Id:
            return QStringLiteral("id");
    }

    return QStringLiteral("title COLLATE NOCASE");
}

void appendIdPredicate(
    const std::vector<std::int64_t> &ids,
    QStringList &predicates,
    QList<QPair<QString, QVariant>> &bindings)
{
    if (ids.empty())
    {
        return;
    }

    QStringList placeholders;
    placeholders.reserve(static_cast<qsizetype>(ids.size()));

    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        const QString placeholder = QStringLiteral(":game_id_%1").arg(
            static_cast<qulonglong>(index));
        placeholders.push_back(placeholder);
        bindings.push_back({
            placeholder,
            QVariant::fromValue<qlonglong>(static_cast<qlonglong>(ids[index]))});
    }

    predicates.push_back(
        QStringLiteral("id IN (%1)").arg(placeholders.join(QStringLiteral(", "))));
}

} // namespace

GameRepository::GameRepository(const QSqlDatabase &database)
    : database_{database}
{}

std::vector<domain::Game>
GameRepository::query(const GameQuery &specification) const
{
    QString sql = QStringLiteral(
        "SELECT id, title, executable_path, executable_name, steam_app_id, "
        "artwork_path, tracking_enabled FROM games");

    QStringList predicates;
    QList<QPair<QString, QVariant>> bindings;
    appendIdPredicate(specification.ids, predicates, bindings);

    const auto addEquality = [&predicates, &bindings](
                                 const QString &column,
                                 const QString &placeholder,
                                 const QVariant &value) {
        predicates.push_back(column + QStringLiteral(" = ") + placeholder);
        bindings.push_back({placeholder, value});
    };

    if (specification.title)
    {
        addEquality(
            QStringLiteral("title COLLATE NOCASE"),
            QStringLiteral(":title"),
            *specification.title);
    }

    if (specification.executableName)
    {
        addEquality(
            QStringLiteral("executable_name"),
            QStringLiteral(":executable_name"),
            *specification.executableName);
    }

    if (specification.executablePath)
    {
        addEquality(
            QStringLiteral("executable_path"),
            QStringLiteral(":executable_path"),
            *specification.executablePath);
    }

    if (specification.steamAppId)
    {
        addEquality(
            QStringLiteral("steam_app_id"),
            QStringLiteral(":steam_app_id"),
            *specification.steamAppId);
    }

    if (specification.trackingEnabled)
    {
        addEquality(
            QStringLiteral("tracking_enabled"),
            QStringLiteral(":tracking_enabled"),
            *specification.trackingEnabled);
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
        // SQLite requires LIMIT when OFFSET is present. -1 means no upper limit.
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
            << "Failed to prepare game query:" << sqlQuery.lastError().text();
        return {};
    }

    for (const auto &[placeholder, value] : bindings)
    {
        sqlQuery.bindValue(placeholder, value);
    }

    if (!sqlQuery.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to execute game query:" << sqlQuery.lastError().text();
        return {};
    }

    std::vector<domain::Game> games;
    while (sqlQuery.next())
    {
        games.push_back(gameFromQuery(sqlQuery));
    }
    return games;
}

bool GameRepository::insert(domain::Game &game)
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "INSERT INTO games "
        "(title, executable_path, executable_name, steam_app_id, artwork_path, "
        "tracking_enabled) VALUES (:title, :executable_path, :executable_name, "
        ":steam_app_id, :artwork_path, :tracking_enabled)"));

    query.bindValue(QStringLiteral(":title"), game.title);
    query.bindValue(QStringLiteral(":executable_path"), game.executablePath);
    query.bindValue(QStringLiteral(":executable_name"), game.executableName);
    bindNullableInt(query, QStringLiteral(":steam_app_id"), game.steamAppId);
    bindNullableString(query, QStringLiteral(":artwork_path"), game.artworkPath);
    query.bindValue(QStringLiteral(":tracking_enabled"), game.trackingEnabled);

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to insert game:" << query.lastError().text();
        return false;
    }

    game.id = query.lastInsertId().toInt();
    return true;
}

bool GameRepository::update(const domain::Game &game)
{
    QSqlQuery query{database_};
    if(!query.prepare(QStringLiteral(
        "UPDATE games "
        "SET title = :title, "
        "executable_path = :executable_path, "
        "executable_name = :executable_name, "
        "steam_app_id = :steam_app_id, "
        "artwork_path = :artwork_path, "
        "tracking_enabled = :tracking_enabled "
        "WHERE id = :game_id")))
    {
        qCWarning(gamelogDatabaseLog) << "Failed to prepare game update:" << query.lastError().text();
        return false;
    }
    query.bindValue(QStringLiteral(":title"), game.title);
    query.bindValue(QStringLiteral(":executable_path"), game.executablePath);
    query.bindValue(QStringLiteral(":executable_name"), game.executableName);
    bindNullableInt(query, QStringLiteral(":steam_app_id"), game.steamAppId);
    bindNullableString(query, QStringLiteral(":artwork_path"), game.artworkPath);
    query.bindValue(QStringLiteral(":tracking_enabled"), game.trackingEnabled);
    query.bindValue(QStringLiteral(":game_id"), game.id);

    qDebug() << "SQL:" << query.lastQuery();
    qDebug() << "Bound value count:" << query.boundValues().size();
    qDebug() << "Bound values:" << query.boundValues();

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to update game:" << query.lastError().text();
        return false;
    }

    return true;
}

bool GameRepository::remove(std::int64_t id)
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral("DELETE FROM games WHERE id = :id"));
    query.bindValue(
        QStringLiteral(":id"),
        QVariant::fromValue<qlonglong>(static_cast<qlonglong>(id)));

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to delete game:" << query.lastError().text();
        return false;
    }

    return true;
}

} // namespace gamelog::core::database
