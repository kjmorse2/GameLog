#pragma once

#include "artwork/ArtworkProvider.h"

namespace gamelog::core::artwork
{
class SteamArtworkProvider final : public ArtworkProvider
{
public:
    [[nodiscard]] std::optional<QString> artworkPathForGame(const domain::Game &game) const override
    {
        Q_UNUSED(game);
        // TODO: Resolve Steam artwork from local metadata/cache.
        return std::nullopt;
    }
};
} // namespace gamelog::core::artwork
