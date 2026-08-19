#include "database/GameRepository.h"
#include "database/SqlQueryBuilder.h"

#include "logging/LoggingCategories.h"

#include <optional>

#include <QList>
#include <QPair>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace gamelog::core::database
{
    using domain::Game;

    namespace
    {
        using domain::query::GameQuery;
        using domain::query::GameSortField;
        using domain::query::SortDirection;

        Game gameFromQuery(const QSqlQuery& query)
        {
            Game game;
            game.id = query.value(QStringLiteral("id")).toInt();
            game.title = query.value(QStringLiteral("title")).toString();
            game.executablePath = query.value(QStringLiteral("executable_path")).toString();
            game.executableName = query.value(QStringLiteral("executable_name")).toString();

            const QVariant steamAppId = query.value(QStringLiteral("steam_app_id"));
            if(!steamAppId.isNull()) { game.steamAppId = steamAppId.toInt(); }

            game.hasArtwork = query.value(QStringLiteral("has_artwork")).toBool();
            game.trackingEnabled = query.value(QStringLiteral("tracking_enabled")).toBool();
            return game;
        }

        void bindNullableInt(QSqlQuery& query, const QString& placeholder, const std::optional<int>& value)
        {
            query.bindValue(placeholder, value ? QVariant{*value} : QVariant{});
        }

        /// Field rules shared by insert and update; the ID rule differs and is checked by each caller.
        bool validateGameFields(const Game& game)
        {
            if(game.title.trimmed().isEmpty())
            {
                qCWarning(gamelogDatabaseLog) << "Refusing to persist a game with an empty or whitespace-only title.";
                return false;
            }

            if(game.steamAppId && *game.steamAppId <= 0)
            {
                qCWarning(gamelogDatabaseLog) << "Refusing to persist a game with a non-positive Steam App ID:" << *game
                   .steamAppId;
                return false;
            }

            return true;
        }

        bool validateGameForInsert(const Game& game)
        {
            if(game.id != 0)
            {
                qCWarning(gamelogDatabaseLog) << "Refusing to insert a game with a preassigned ID:" << game.id;
                return false;
            }

            return validateGameFields(game);
        }

        bool validateGameForUpdate(const Game& game)
        {
            if(game.id <= 0)
            {
                qCWarning(gamelogDatabaseLog) << "Refusing to update a game without a valid ID:" << game.id;
                return false;
            }

            return validateGameFields(game);
        }

        QString orderColumn(GameSortField field)
        {
            switch(field)
            {
            case GameSortField::Title:
                return QStringLiteral("title COLLATE NOCASE");
            case GameSortField::Id:
                return QStringLiteral("id");
            }

            return QStringLiteral("title COLLATE NOCASE");
        }
    } // namespace

    GameRepository::GameRepository(const QSqlDatabase& database) : database_{database} {}

    std::vector<domain::Game> GameRepository::query(const GameQuery& specification) const
    {
        const QString baseSql = QStringLiteral("SELECT id, title, executable_path, executable_name, steam_app_id, "
                                               "has_artwork, tracking_enabled FROM games");

        SqlQueryBuilder builder;

        QList<QVariant> ids;
        ids.reserve(static_cast<qsizetype>(specification.ids.size()));
        for(const int id : specification.ids) { ids.push_back(QVariant::fromValue<qlonglong>(id)); }
        builder.addInPredicate(QStringLiteral("id"), QStringLiteral("game_id"), ids);

        const auto addEquality = [&builder](const QString& column, const QString& placeholder, const QVariant& value)
        {
            builder.addPredicate(column + QStringLiteral(" = ") + placeholder, placeholder, value);
        };

        if(specification.title)
        {
            addEquality(QStringLiteral("title COLLATE NOCASE"), QStringLiteral(":title"), *specification.title);
        }

        if(specification.executableName)
        {
            addEquality(QStringLiteral("executable_name"),
                        QStringLiteral(":executable_name"),
                        *specification.executableName);
        }

        if(specification.executablePath)
        {
            addEquality(QStringLiteral("executable_path"),
                        QStringLiteral(":executable_path"),
                        *specification.executablePath);
        }

        if(specification.steamAppId)
        {
            addEquality(QStringLiteral("steam_app_id"), QStringLiteral(":steam_app_id"), *specification.steamAppId);
        }

        if(specification.trackingEnabled)
        {
            addEquality(QStringLiteral("tracking_enabled"),
                        QStringLiteral(":tracking_enabled"),
                        *specification.trackingEnabled);
        }

        builder.setOrderBy(orderColumn(specification.sortBy), specification.sortDirection);
        builder.setLimitOffset(specification.limit, specification.offset);

        const QString sql = builder.buildSql(baseSql);

        QSqlQuery sqlQuery{database_};
        if(!sqlQuery.prepare(sql))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to prepare game query:" << sqlQuery.lastError().text();
            return {};
        }

        builder.bindTo(sqlQuery);

        if(!sqlQuery.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to execute game query:" << sqlQuery.lastError().text();
            return {};
        }

        std::vector<domain::Game> games;
        while(sqlQuery.next()) { games.push_back(gameFromQuery(sqlQuery)); }
        return games;
    }

    bool GameRepository::insert(domain::Game& game)
    {
        if(!validateGameForInsert(game)) { return false; }

        QSqlQuery query{database_};
        if(!query.prepare(QStringLiteral("INSERT INTO games "
                                         "(title, executable_path, executable_name, steam_app_id, has_artwork, "
                                         "tracking_enabled) VALUES (:title, :executable_path, :executable_name, "
                                         ":steam_app_id, :has_artwork, :tracking_enabled)")))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to prepare game insert:" << query.lastError().text();
            return false;
        }

        query.bindValue(QStringLiteral(":title"), game.title);
        query.bindValue(QStringLiteral(":executable_path"), game.executablePath);
        query.bindValue(QStringLiteral(":executable_name"), game.executableName);
        bindNullableInt(query, QStringLiteral(":steam_app_id"), game.steamAppId);
        query.bindValue(QStringLiteral(":has_artwork"), game.hasArtwork);
        query.bindValue(QStringLiteral(":tracking_enabled"), game.trackingEnabled);

        if(!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to insert game:" << query.lastError().text();
            return false;
        }

        const QVariant insertedId = query.lastInsertId();
        if(!insertedId.isValid() || insertedId.toLongLong() <= 0)
        {
            qCWarning(gamelogDatabaseLog) << "Game insert returned no valid primary key.";
            return false;
        }

        game.id = insertedId.toInt();
        return true;
    }

    bool GameRepository::update(const domain::Game& game)
    {
        if(!validateGameForUpdate(game)) { return false; }

        QSqlQuery query{database_};
        if(!query.prepare(QStringLiteral("UPDATE games SET " "title = :title, " "executable_path = :executable_path, "
                                         "executable_name = :executable_name, " "steam_app_id = :steam_app_id, "
                                         "has_artwork = :has_artwork, " "tracking_enabled = :tracking_enabled "
                                         "WHERE id = :game_id")))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to prepare game update:" << query.lastError().text();
            return false;
        }

        query.bindValue(QStringLiteral(":title"), game.title);
        query.bindValue(QStringLiteral(":executable_path"), game.executablePath);
        query.bindValue(QStringLiteral(":executable_name"), game.executableName);
        bindNullableInt(query, QStringLiteral(":steam_app_id"), game.steamAppId);
        query.bindValue(QStringLiteral(":has_artwork"), game.hasArtwork);
        query.bindValue(QStringLiteral(":tracking_enabled"), game.trackingEnabled);
        query.bindValue(QStringLiteral(":game_id"), game.id);

        if(!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to update game:" << query.lastError().text();
            return false;
        }

        return query.numRowsAffected() == 1;
    }

    bool GameRepository::remove(int id)
    {
        if(id <= 0) { return false; }

        QSqlQuery query{database_};
        query.prepare(QStringLiteral("DELETE FROM games WHERE id = :id"));
        query.bindValue(QStringLiteral(":id"), QVariant::fromValue<qlonglong>(id));

        if(!query.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to delete game:" << query.lastError().text();
            return false;
        }

        return query.numRowsAffected() == 1;
    }
} // namespace gamelog::core::database
