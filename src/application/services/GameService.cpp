#include "application/services/GameService.h"

#include <utility>

using gamelog::core::domain::query::GameQuery;

namespace gamelog::application::services {
namespace {

template<typename T>
optional<T> firstOrNull(vector<T> values)
{
    if (values.empty())
    {
        return std::nullopt;
    }
    return std::move(values.front());
}

} // namespace

GameService::GameService(core::database::GameRepository &repository)
    : repository_{repository}
{}

vector<Game> GameService::search(const GameQuery &query) const
{
    return repository_.query(query);
}

void GameService::listGames() const
{
    emit gamesFound(search({}));
}

vector<Game> GameService::listTrackedGames() const
{
    GameQuery query;
    query.trackingEnabled = true;
    return search(query);
}

optional<Game> GameService::findById(std::int64_t id) const
{
    GameQuery query;
    query.ids = {id};
    query.limit = 1;
    return firstOrNull(search(query));
}

optional<Game> GameService::findByExecutableName(const QString &name) const
{
    GameQuery query;
    query.executableName = name;
    query.limit = 1;
    return firstOrNull(search(query));
}

optional<Game> GameService::findByExecutablePath(const QString &path) const
{
    GameQuery query;
    query.executablePath = path;
    query.limit = 1;
    return firstOrNull(search(query));
}

bool GameService::addGame(Game &game)
{
    return repository_.insert(game);
}

bool GameService::updateGame(const Game &game)
{
    return repository_.update(game);
}

bool GameService::removeGame(std::int64_t id)
{
    return repository_.remove(id);
}

} // namespace gamelog::application::services
