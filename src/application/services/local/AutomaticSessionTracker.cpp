#include "AutomaticSessionTracker.h"

#include <algorithm>
#include <ranges>

#include "process/ProcessHelpers.h"

using gamelog::core::domain::Game;
using gamelog::core::process::ProcessHelpers;
using gamelog::core::process::ProcessInfo;
using std::chrono::seconds;

namespace gamelog::application::services
{
    TrackingDecision AutomaticSessionTracker::advance(const std::vector<ProcessInfo>& processes,
                                                      const seconds elapsed,
                                                      const Game* activeGame,
                                                      const QHash<std::uint32_t, Game>& trackedSteamGames,
                                                      const QHash<QString, Game>& trackedPathGames)
    {
        if(elapsed <= seconds::zero()) { return {}; }

        if(activeGame == nullptr)
        {
            const std::optional<Game> detectedGame = selectDetectedGame(processes,
                                                                        trackedSteamGames,
                                                                        trackedPathGames);
            if(!detectedGame)
            {
                resetPendingStart();
                return {};
            }

            if(!pendingGameId_ || *pendingGameId_ != detectedGame->id)
            {
                pendingGameId_ = detectedGame->id;
                gameOpenDuration_ = seconds::zero();
            }

            gameOpenDuration_ += elapsed;
            if(gameOpenDuration_ < kStartGracePeriod) { return {}; }

            resetPendingStart();
            return {TrackingAction::Start, detectedGame};
        }

        const bool activeGameFound = std::ranges::any_of(processes,
                                                         [activeGame](const ProcessInfo& process)
                                                         {
                                                             return ProcessHelpers::processMatchesGame(process,
                                                                 *activeGame);
                                                         });

        if(activeGameFound)
        {
            gameClosedDuration_ = seconds::zero();
            return {};
        }

        gameClosedDuration_ += elapsed;
        if(gameClosedDuration_ < kEndGracePeriod) { return {}; }

        gameClosedDuration_ = seconds::zero();
        return {TrackingAction::Stop, std::nullopt};
    }

    void AutomaticSessionTracker::reset() noexcept
    {
        resetPendingStart();
        gameClosedDuration_ = seconds::zero();
    }

    std::optional<Game> AutomaticSessionTracker::selectDetectedGame(
        const std::vector<ProcessInfo>& processes,
        const QHash<std::uint32_t, Game>& trackedSteamGames,
        const QHash<QString, Game>& trackedPathGames) const
    {
        struct Candidate
        {
            Game game;
            int priority;
        };

        std::vector<Candidate> candidates;

        const auto addCandidate = [&candidates](const Game& game, int priority)
        {
            const auto existing = std::find_if(candidates.begin(),
                                               candidates.end(),
                                               [&game](const Candidate& candidate)
                                               {
                                                   return candidate.game.id == game.id;
                                               });

            if(existing == candidates.end()) { candidates.push_back({game, priority}); }
            else { existing->priority = std::min(existing->priority, priority); }
        };

        for(const ProcessInfo& process : processes)
        {
            // Precedence lives in ProcessHelpers (contract item 33). Selection
            // policy below only ranks what the matcher already classified.
            const auto [game, kind] = ProcessHelpers::matchTrackedGame(process, trackedSteamGames, trackedPathGames);

            switch(kind)
            {
            case core::process::MatchKind::SteamAppId:
                addCandidate(*game, 0);
                break;
            case core::process::MatchKind::ExecutablePath:
                addCandidate(*game, 1);
                break;
            case core::process::MatchKind::None:
                break;
            }
        }

        if(candidates.empty()) { return std::nullopt; }

        if(pendingGameId_)
        {
            const auto pending = std::find_if(candidates.begin(),
                                              candidates.end(),
                                              [this](const Candidate& candidate)
                                              {
                                                  return candidate.game.id == *pendingGameId_;
                                              });
            if(pending != candidates.end()) { return pending->game; }
        }

        const auto selected = std::min_element(candidates.begin(),
                                               candidates.end(),
                                               [](const Candidate& left, const Candidate& right)
                                               {
                                                   if(left.priority != right.priority)
                                                   {
                                                       return left.priority < right.priority;
                                                   }

                                                   return left.game.id < right.game.id;
                                               });

        return selected->game;
    }

    void AutomaticSessionTracker::resetPendingStart() noexcept
    {
        pendingGameId_.reset();
        gameOpenDuration_ = seconds::zero();
    }
} // namespace gamelog::application::services
