#pragma once

#include <optional>
#include <vector>

#include "domain/Game.h"

namespace gamelog::core::database
{
class GameRepository
{
public:
    virtual ~GameRepository() = default;

    [[nodiscard]] virtual std::optional<domain::Game> findById(int id) = 0;
    [[nodiscard]] virtual std::vector<domain::Game> listGames() = 0;
    virtual bool save(const domain::Game &game) = 0;
};
} // namespace gamelog::core::database
