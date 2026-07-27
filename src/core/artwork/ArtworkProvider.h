#pragma once

#include <optional>

#include <QString>

#include "domain/Game.h"

namespace gamelog::core::artwork
{
class ArtworkProvider
{
public:
    virtual ~ArtworkProvider() = default;
    [[nodiscard]] virtual std::optional<QString> artworkPathForGame(const domain::Game &game) const = 0;
};
} // namespace gamelog::core::artwork
