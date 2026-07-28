#pragma once

#include <optional>
#include <vector>

#include "domain/Game.h"
#include <QtSql/qsqldatabase.h>

namespace gamelog::core::database
{
class GameRepository
{
public:
    explicit GameRepository(QSqlDatabase database);

    [[nodiscard]] std::optional<domain::Game> findById(std::int64_t id) const;
    [[nodiscard]] std::vector<domain::Game> findAll() const;

    bool insert(domain::Game& game);
    bool update(const domain::Game& game);
    bool remove(std::int64_t id);

private:
    QSqlDatabase database_;
};
} // namespace gamelog::core::database
