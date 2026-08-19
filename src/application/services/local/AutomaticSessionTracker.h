#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include <QHash>
#include <QString>

#include "domain/Game.h"
#include "process/ProcessInfo.h"

namespace gamelog::application::services
{
    /**
     * What the tracker concluded should happen to the current session.
     */
    enum class TrackingAction
    {
        None, Start, Stop
    };

    /**
     * One tracking decision. game is set only for TrackingAction::Start, and
     * names the game whose session should begin.
     */
    struct TrackingDecision
    {
        TrackingAction action{TrackingAction::None};
        std::optional<core::domain::Game> game;
    };

    /**
     * Decides when an automatic session should start or stop from process snapshots.
     *
     * This is the grace-period state machine only: it owns no database, emits no
     * signals, and performs no I/O, so it can be exercised directly with
     * synthetic process lists. It decides *what should happen*; SessionService
     * remains responsible for carrying the decision out and for persistence.
     *
     * A session starts once a single tracked game has been detected continuously
     * for kStartGracePeriod, and stops once the active game has been absent
     * continuously for kEndGracePeriod. Detection that flaps between games resets
     * the start timer, so a game must hold the candidacy for the full period.
     */
    class AutomaticSessionTracker
    {
    public:
        /**
         * Advances the state machine by one poll.
         *
         * @param processes The current process snapshot.
         * @param elapsed Time since the previous advance. Values at or below zero
         * are ignored and produce TrackingAction::None.
         * @param activeGame The game of the session in progress, or nullptr when
         * no session is active. Its lifetime need only span the call.
         * @return The action the caller should take.
         */
        [[nodiscard]] TrackingDecision advance(const std::vector<core::process::ProcessInfo>& processes,
                                               std::chrono::seconds elapsed,
                                               const core::domain::Game* activeGame,
                                               const QHash<std::uint32_t, core::domain::Game>& trackedSteamGames,
                                               const QHash<QString, core::domain::Game>& trackedPathGames);

        /**
         * Clears all grace-period state without producing a decision.
         */
        void reset() noexcept;

        /**
         * @brief The game currently accumulating the start grace period, if any.
         *
         * Exposed for tests and diagnostics; callers should not drive behavior from it.
         */
        [[nodiscard]] std::optional<int> pendingGameId() const noexcept { return pendingGameId_; }

    private:
        /**
         * Chooses one game from a process snapshot deterministically. A still
         * detected pending game is retained; otherwise Steam matches precede
         * path-only matches, with game ID used as a tie-breaker.
         */
        [[nodiscard]] std::optional<core::domain::Game> selectDetectedGame(
            const std::vector<core::process::ProcessInfo>& processes,
            const QHash<std::uint32_t, core::domain::Game>& trackedSteamGames,
            const QHash<QString, core::domain::Game>& trackedPathGames) const;

        /**
         * Reset the pending start state, clearing any pending game ID and
         * resetting the grace-period timer.
         */
        void resetPendingStart() noexcept;

        /**
         * @brief The game ID currently accumulating the automatic-start grace period.
         */
        std::optional<int> pendingGameId_;

        /**
         * @brief The amount of time the pending game has remained detected.
         */
        std::chrono::seconds gameOpenDuration_{0};

        /**
         * @brief The amount of time the active game has remained undetected.
         */
        std::chrono::seconds gameClosedDuration_{0};

        /**
         * @brief The grace period for starting a new automatic session.
         */
        static constexpr std::chrono::seconds kStartGracePeriod{30};

        /**
         * @brief The grace period for ending an automatic session.
         */
        static constexpr std::chrono::seconds kEndGracePeriod{30};
    };
} // namespace gamelog::application::services
