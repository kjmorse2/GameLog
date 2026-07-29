#include "database/GameRepository.h"

#include "logging/LoggingCategories.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace gamelog::core::database
{
namespace
{

// Map one SELECT row into the domain type in one place so every query stays consistent.
domain::Game gameFromQuery(const QSqlQuery& query)
{
    domain::Game game;
    game.id = query.value("id").toInt();
    game.title = query.value("title").toString();
    game.executablePath = query.value("executable_path").toString();
    game.executableName = query.value("executable_name").toString();

    const QVariant steamAppId = query.value("steam_app_id");
    if (!steamAppId.isNull())
    {
        game.steamAppId = steamAppId.toInt();
    }

    const QVariant artworkPath = query.value("artwork_path");
    if (!artworkPath.isNull())
    {
        game.artworkPath = artworkPath.toString();
    }

    game.trackingEnabled = query.value("tracking_enabled").toBool();

    return game;
}

void bindNullableInt(
    QSqlQuery& query,
    const QString& placeholder,
    const std::optional<int>& value
)
{
    // SQLite stores nulls explicitly, so use an empty QVariant when unset.
    if (value.has_value())
    {
        query.bindValue(placeholder, *value);
    }
    else
    {
        query.bindValue(placeholder, QVariant{});
    }
}

void bindNullableString(
    QSqlQuery& query,
    const QString& placeholder,
    const std::optional<QString>& value
)
{
    // Mirror the integer binder for optional text columns.
    if (value.has_value())
    {
        query.bindValue(placeholder, *value);
    }
    else
    {
        query.bindValue(placeholder, QVariant{});
    }
}

} // namespace

GameRepository::GameRepository(QSqlDatabase database)
    : database_{std::move(database)}
{
}

std::vector<domain::Game>
GameRepository::findAll() const
{
    // Pull the full library in title order for UI lists and agent caches.
    QSqlQuery query{database_};

    if (!query.exec(
            R"(
                SELECT
                    id,
                    title,
                    executable_path,
                    executable_name,
                    steam_app_id,
                    artwork_path,
                    tracking_enabled
                FROM games
                ORDER BY title COLLATE NOCASE
            )"))
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to list games:"
            << query.lastError().text();

        return {};
    }

    std::vector<domain::Game> games;

    while (query.next())
    {
        games.push_back(gameFromQuery(query));
    }

    return games;
}

std::optional<domain::Game>
GameRepository::findById(std::int64_t id) const
{
    // Parameterize the lookup so callers never have to build SQL manually.
    QSqlQuery query{database_};
    query.prepare(
        R"(
            SELECT
                id,
                title,
                executable_path,
                executable_name,
                steam_app_id,
                artwork_path,
                tracking_enabled
            FROM games
            WHERE id = :id
        )"
    );
    query.bindValue(
        ":id",
        QVariant::fromValue<qlonglong>(
            static_cast<qlonglong>(id)
        )
    );

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to find game by ID:"
            << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
    {
        return std::nullopt;
    }

    return gameFromQuery(query);
}

bool GameRepository::insert(domain::Game& game)
{
    // Insert only the mutable columns; the database assigns the primary key.
    QSqlQuery query{database_};
    query.prepare(
        R"(
            INSERT INTO games
            (
                title,
                executable_path,
                executable_name,
                steam_app_id,
                artwork_path,
                tracking_enabled
            )
            VALUES
            (
                :title,
                :executable_path,
                :executable_name,
                :steam_app_id,
                :artwork_path,
                :tracking_enabled
            )
        )"
    );

    query.bindValue(":title", game.title);
    query.bindValue(
        ":executable_path",
        game.executablePath
    );
    query.bindValue(
        ":executable_name",
        game.executableName
    );
    bindNullableInt(
        query,
        ":steam_app_id",
        game.steamAppId
    );
    bindNullableString(
        query,
        ":artwork_path",
        game.artworkPath
    );
    query.bindValue(
        ":tracking_enabled",
        game.trackingEnabled
    );

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to insert game:"
            << query.lastError().text();

        return false;
    }

    game.id = query.lastInsertId().toInt();
    return true;
}

bool GameRepository::update(const domain::Game& game)
{
    // Update the row in place using the in-memory id as the key.
    QSqlQuery query{database_};
    query.prepare(
        R"(
            UPDATE games
            SET
                title = :title,
                executable_path = :executable_path,
                executable_name = :executable_name,
                steam_app_id = :steam_app_id,
                artwork_path = :artwork_path,
                tracking_enabled = :tracking_enabled
            WHERE id = :id
        )"
    );

    query.bindValue(":title", game.title);
    query.bindValue(
        ":executable_path",
        game.executablePath
    );
    query.bindValue(
        ":executable_name",
        game.executableName
    );
    bindNullableInt(
        query,
        ":steam_app_id",
        game.steamAppId
    );
    bindNullableString(
        query,
        ":artwork_path",
        game.artworkPath
    );
    query.bindValue(
        ":tracking_enabled",
        game.trackingEnabled
    );
    query.bindValue(":id", game.id);

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to update game:"
            << query.lastError().text();

        return false;
    }

    return true;
}

bool GameRepository::remove(std::int64_t id)
{
    // Deleting by primary key keeps the repository behavior predictable.
    QSqlQuery query{database_};
    query.prepare(
        "DELETE FROM games WHERE id = :id"
    );
    query.bindValue(
        ":id",
        QVariant::fromValue<qlonglong>(
            static_cast<qlonglong>(id)
        )
    );

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to delete game:"
            << query.lastError().text();

        return false;
    }

    return true;
}

} // namespace gamelog::core::database
