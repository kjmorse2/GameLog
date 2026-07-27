#pragma once

#include <vector>

#include "domain/Game.h"

namespace gamelog::core::steam
{
class SteamLibraryScanner
{
public:
    [[nodiscard]] std::vector<domain::Game> scanInstalledGames() const;
};
} // namespace gamelog::core::steam
