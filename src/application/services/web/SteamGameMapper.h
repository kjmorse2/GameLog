#pragma once

#include <vector>

#include <QJsonArray>

#include "domain/Game.h"

namespace gamelog::application::services
{
    /**
     * Maps a Steam GetOwnedGames "games" array into domain Game values.
     *
     * Parsing only: the result describes what Steam reported, not what should be
     * persisted. Deciding which of these games are new is GameService's policy
     * (contract item 12).
     *
     * Entries that are not objects, carry a non-positive "appid", or whose "name"
     * is blank after trimming are skipped rather than failing the whole payload,
     * because one malformed entry should not discard an entire library.
     *
     * Each returned Game has only title and steamAppId set; id stays zero so the
     * repository assigns it on insert.
     */
    [[nodiscard]] std::vector<core::domain::Game> gamesFromSteamOwnedGames(const QJsonArray& steamGames);
} // namespace gamelog::application::services
