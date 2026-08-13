#include "application/services/GameService.h"

#include "logging/LoggingCategories.h"

#include <utility>

namespace gamelog::application::services
{
    namespace
    {
        template<typename T> std::optional<T> firstOrNull(std::vector<T> values)
        {
            if (values.empty())
            {
                return std::nullopt;
            }
            return std::move(values.front());
        }
    } // namespace

    GameService::GameService(core::database::GameRepository& repository)
        : repository_{repository} {}

    std::vector<core::domain::Game> GameService::search(const core::domain::query::GameQuery& query) const
    {
        return repository_.query(query);
    }

    void GameService::listGames() const
    {
        emit gamesFound(search({}));
    }

    std::vector<core::domain::Game> GameService::listTrackedGames() const
    {
        core::domain::query::GameQuery query;
        query.trackingEnabled = true;
        return search(query);
    }

    std::optional<core::domain::Game> GameService::findById(std::int64_t id) const
    {
        core::domain::query::GameQuery query;
        query.ids = {id};
        query.limit = 1;
        return firstOrNull(search(query));
    }

    std::optional<core::domain::Game> GameService::findByExecutableName(const QString& name) const
    {
        core::domain::query::GameQuery query;
        query.executableName = name;
        query.limit = 1;
        return firstOrNull(search(query));
    }

    std::optional<core::domain::Game> GameService::findByExecutablePath(const QString& path) const
    {
        core::domain::query::GameQuery query;
        query.executablePath = path;
        query.limit = 1;
        return firstOrNull(search(query));
    }

    bool GameService::addGame(core::domain::Game& game)
    {
        if (!repository_.insert(game))
        {
            return false;
        }

        syncGamesWithDatabase();
        return true;
    }

    bool GameService::updateGame(const core::domain::Game& game)
    {
        if (!repository_.update(game))
        {
            return false;
        }

        syncGamesWithDatabase();
        return true;
    }

    bool GameService::removeGame(std::int64_t id)
    {
        if (!repository_.remove(id))
        {
            return false;
        }

        syncGamesWithDatabase();
        return true;
    }

    void GameService::syncGamesWithDatabase()
    {
        trackedPathGames_.clear();
        trackedSteamGames_.clear();

        for (const core::domain::Game& game: listTrackedGames())
        {
            if (game.steamAppId && *game.steamAppId > 0)
            {
                trackedSteamGames_.insert(static_cast<std::uint32_t>(*game.steamAppId), game);
            }
            if (!game.executablePath.isEmpty())
            {
                trackedPathGames_.insert(game.executablePath, game);
            }
        }

        qCInfo(gamelogAgentLog) << "Synced" << trackedSteamGames_.size() << "Steam games and" << trackedPathGames_.size() << "path-based games.";
    }

    const QHash<std::uint32_t, core::domain::Game>& GameService::trackedSteamGames() const noexcept
    {
        return trackedSteamGames_;
    }

    const QHash<QString, core::domain::Game>& GameService::trackedPathGames() const noexcept
    {
        return trackedPathGames_;
    }

    bool GameService::hasTrackedSteamGames() const noexcept
    {
        return !trackedSteamGames_.isEmpty();
    }
} // namespace gamelog::application::services
