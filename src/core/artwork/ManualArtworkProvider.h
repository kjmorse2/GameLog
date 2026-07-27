#pragma once

#include "artwork/ArtworkProvider.h"

namespace gamelog::core::artwork
{
class ManualArtworkProvider final : public ArtworkProvider
{
public:
    [[nodiscard]] std::optional<QString> artworkPathForGame(const domain::Game &game) const override
    {
        Q_UNUSED(game);
        // TODO: Return user-selected manual artwork when configured.
        return std::nullopt;
    }
};
} // namespace gamelog::core::artwork
