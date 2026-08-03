#pragma once

#include <cstdint>
#include <vector>

#include <QSqlDatabase>

#include "domain/Game.h"
#include "domain/query/GameQuery.h"

namespace gamelog::core::database {

/**
 * Translates GameQuery specifications into SQL and persists Game rows.
 */
class GameRepository
{
public:
    explicit GameRepository(const QSqlDatabase &database);

    [[nodiscard]] std::vector<domain::Game>
    query(const domain::query::GameQuery &specification) const;

    [[nodiscard]] bool insert(domain::Game &game);
    [[nodiscard]] bool update(const domain::Game &game);
    [[nodiscard]] bool remove(std::int64_t id);

private:
    QSqlDatabase database_;
};

} // namespace gamelog::core::database
