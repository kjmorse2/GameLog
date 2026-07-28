#include "GameRepository.h"
#include <QSqlQuery>
#include <QLoggingCategory>
#include <utility>

#include "logging/LoggingCategories.h"
#include <QtCore/qloggingcategory.h>


namespace gamelog::core::database
{
    GameRepository::GameRepository(QSqlDatabase database)
        : database_(std::move(database))
    {
    }

    std::vector<domain::Game> GameRepository::findAll() const
    {
        std::vector<domain::Game> games;
        
        QSqlQuery query(database_);
        query.prepare("SELECT id, title, executable_path, executable_name, steam_app_id, artwork_path, tracking_enabled FROM games");
        if (!query.exec())
        { 
            qCWarning(gamelogDatabaseLog) << "Failed to execute query:" << query.lastError().text();
            return games;
        }

        while (query.next())
        {
            domain::Game game;
            game.id = query.value(0).toInt();
            game.title = query.value(1).toString();
            game.executablePath = query.value(2).toString();
            game.executableName = query.value(3).toString();
            game.steamAppId = query.value(4).isNull() ? std::nullopt : std::make_optional(query.value(4).toInt());
            game.artworkPath = query.value(5).isNull() ? std::nullopt : std::make_optional(query.value(5).toString());
            game.trackingEnabled = query.value(6).toBool();

            games.push_back(std::move(game));
        }

        return games;
    }

    std::optional<domain::Game> GameRepository::findById(std::int64_t id) const
    {
        std::vector<domain::Game> games;
        
        QSqlQuery query(database_);
        query.prepare("SELECT id, title, executable_path, executable_name, steam_app_id, artwork_path, tracking_enabled FROM games where id = :id");
        query.bindValue(":id", id);
        if (!query.exec())
        { 
            qCWarning(gamelogDatabaseLog) << "Failed to execute query:" << query.lastError().text();
        }

        if (query.next())
        {
            domain::Game game;
            game.id = query.value(0).toInt();
            game.title = query.value(1).toString();
            game.executablePath = query.value(2).toString();
            game.executableName = query.value(3).toString();
            game.steamAppId = query.value(4).isNull() ? std::nullopt : std::make_optional(query.value(4).toInt());
            game.artworkPath = query.value(5).isNull() ? std::nullopt : std::make_optional(query.value(5).toString());
            game.trackingEnabled = query.value(6).toBool();

            return std::make_optional(std::move(game));
        }

        return std::nullopt;
    }

    bool GameRepository::insert(domain::Game& game)
    {
        QSqlQuery query(database_);
        query.prepare("INSERT INTO games (title, executable_path, executable_name, steam_app_id, artwork_path, tracking_enabled) "
                      "VALUES (:title, :executable_path, :executable_name, :steam_app_id, :artwork_path, :tracking_enabled)");
        query.bindValue(":title", game.title);
        query.bindValue(":executable_path", game.executablePath);
        query.bindValue(":executable_name", game.executableName);
        query.bindValue(":steam_app_id", game.steamAppId.has_value() ? QVariant(*game.steamAppId) : QVariant(QVariant::Int));
        query.bindValue(":artwork_path", game.artworkPath.has_value() ? QVariant(*game.artworkPath) : QVariant(QVariant::String));
        query.bindValue(":tracking_enabled", game.trackingEnabled);

        if (!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to execute insert query:" << query.lastError().text();
            return false;
        }

        game.id = query.lastInsertId().toInt();
        return true;
    }

    bool GameRepository::update(const domain::Game& game)
    {
        QSqlQuery query(database_);
        query.prepare("UPDATE games SET title = :title, executable_path = :executable_path, executable_name = :executable_name, "
                      "steam_app_id = :steam_app_id, artwork_path = :artwork_path, tracking_enabled = :tracking_enabled "
                      "WHERE id = :id");
        query.bindValue(":title", game.title);
        query.bindValue(":executable_path", game.executablePath);
        query.bindValue(":executable_name", game.executableName);
        query.bindValue(":steam_app_id", game.steamAppId.has_value() ? QVariant(*game.steamAppId) : QVariant(QVariant::Int));
        query.bindValue(":artwork_path", game.artworkPath.has_value() ? QVariant(*game.artworkPath) : QVariant(QVariant::String));
        query.bindValue(":tracking_enabled", game.trackingEnabled);
        query.bindValue(":id", game.id);

        if (!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to execute update query:" << query.lastError().text();
            return false;
        }

        return true;
    }

    bool GameRepository::remove(std::int64_t id)
    {
        QSqlQuery query(database_);
        query.prepare("DELETE FROM games WHERE id = :id");
        query.bindValue(":id", id);

        if (!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to execute delete query:" << query.lastError().text();
            return false;
        }
        return true;
    }
}