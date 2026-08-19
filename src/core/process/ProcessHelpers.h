#pragma once

#include <cstdint>
#include <optional>

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QtTypes>

#include "domain/Game.h"

namespace gamelog::core::process
{
    struct ProcessInfo;

    /**
     * How a process was identified as a tracked game.
     *
     * Steam identity outranks executable-path identity; callers that need to
     * prefer one candidate over another rely on this ordering rather than
     * re-deriving it. See CONTRACT_CHANGES.md item 33.
     */
    enum class MatchKind
    {
        None, SteamAppId, ExecutablePath
    };

    /**
     * The outcome of matching one process against the tracked-game indexes.
     *
     * game is null exactly when kind is MatchKind::None. It points into one of
     * the hashes passed to matchTrackedGame() and stays valid only while those
     * hashes are unmodified.
     */
    struct TrackedGameMatch
    {
        const domain::Game* game{nullptr};
        MatchKind kind{MatchKind::None};
    };

    class ProcessHelpers
    {
    public:
        /**
         * Reads an environment variable for a process with a provided pid.
         */
        [[nodiscard]] static std::optional<QByteArray> readProcessEnvironmentValue(
            qint64 pid,
            const QByteArray& variableName);

        /**
         * Reads the Steam App ID for a process with a provided pid.
         */
        [[nodiscard]] static std::optional<std::uint32_t> readSteamAppId(qint64 pid);

        /**
         * Finds the tracked game matching a process without copying either index.
         * When both a process and a path candidate have Steam App IDs, those IDs
         * are authoritative: a mismatch does not fall back to the matching path.
         * The returned pointer refers to an element in one of the supplied hashes
         * and remains valid only while those hashes are not modified.
         *
         * This is the single implementation of the precedence rule in contract
         * item 33; callers needing to rank candidates read the returned kind
         * rather than repeating the lookup order themselves.
         */
        [[nodiscard]] static TrackedGameMatch matchTrackedGame(const ProcessInfo& process,
                                                               const QHash<std::uint32_t, domain::Game>&
                                                               trackedSteamGames,
                                                               const QHash<QString, domain::Game>& trackedPathGames)
            noexcept;

        /**
         * Checks whether one process corresponds to one game. Matching Steam IDs
         * take precedence when both sides provide them; otherwise the executable
         * path is used.
         */
        [[nodiscard]] static bool processMatchesGame(const ProcessInfo& process, const domain::Game& game) noexcept;
    };
} // namespace gamelog::core::process
