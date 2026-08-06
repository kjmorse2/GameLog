#include "application/services/GameService.h"

#include <utility>

namespace gamelog::application::services {
namespace {

template<typename T>
std::optional<T> firstOrNull(std::vector<T> values)
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

std::vector<core::domain::Game>
GameService::search(const core::domain::query::GameQuery &query) const
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

std::optional<core::domain::Game>
GameService::findById(std::int64_t id) const
{
    core::domain::query::GameQuery query;
    query.ids = {id};
    query.limit = 1;
    return firstOrNull(search(query));
}

std::optional<core::domain::Game>
GameService::findByExecutableName(const QString &name) const
{
    core::domain::query::GameQuery query;
    query.executableName = name;
    query.limit = 1;
    return firstOrNull(search(query));
}

std::optional<core::domain::Game>
GameService::findByExecutablePath(const QString &path) const
{
    core::domain::query::GameQuery query;
    query.executablePath = path;
    query.limit = 1;
    return firstOrNull(search(query));
}

bool GameService::addGame(core::domain::Game &game)
{
    return repository_.insert(game);
}

bool GameService::updateGame(const core::domain::Game &game)
{
    return repository_.update(game);
}

bool GameService::removeGame(std::int64_t id)
{
    return repository_.remove(id);
}

} // namespace gamelog::application::services
