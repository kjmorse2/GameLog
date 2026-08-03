#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QString>

#include "database/GameRepository.h"
#include "domain/Game.h"
#include "domain/query/GameQuery.h"

namespace gamelog::application::services {

/**
 * Application-facing game operations.
 *
 * Callers describe general searches with GameQuery or use the semantic
 * convenience methods. SQL remains entirely behind GameRepository.
 */
class GameService
{
public:
    explicit GameService(core::database::GameRepository &repository);

    [[nodiscard]] std::vector<core::domain::Game>
    search(const core::domain::query::GameQuery &query) const;

    [[nodiscard]] std::vector<core::domain::Game> listGames() const;
    [[nodiscard]] std::vector<core::domain::Game> listTrackedGames() const;
    [[nodiscard]] std::optional<core::domain::Game>
    findById(std::int64_t id) const;
    [[nodiscard]] std::optional<core::domain::Game>
    findByExecutableName(const QString &name) const;
    [[nodiscard]] std::optional<core::domain::Game>
    findByExecutablePath(const QString &path) const;

    [[nodiscard]] bool addGame(core::domain::Game &game);
    [[nodiscard]] bool updateGame(const core::domain::Game &game);
    [[nodiscard]] bool removeGame(std::int64_t id);

private:
    core::database::GameRepository &repository_;
};

} // namespace gamelog::application::services
