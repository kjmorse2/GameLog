#include "application/GameLogRuntime.h"

#include "logging/LoggingCategories.h"
#include "process/ProcessSource.h"
#include "process/ProcfsProcessSource.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <QSqlDatabase>

using gamelog::core::process::ProcessInfo;
namespace d = gamelog::core::domain;
using std::chrono::seconds;

namespace gamelog::application {

GameLogRuntime::GameLogRuntime(QString databasePath)
    : databaseManager_{ std::move(databasePath),
    QStringLiteral("GameLogRuntimeConnection")}
{
    databaseReady_ = databaseManager_.initialize();
    if (!databaseReady_)
    {
        qCWarning(gamelogAgentLog) << "Failed to initialize the database manager.";
        return;
    }

    const QSqlDatabase database = databaseManager_.database();
    gameRepository_.emplace(database);
    sessionRepository_.emplace(database);
    gameService_.emplace(*gameRepository_);
    sessionService_.emplace(*sessionRepository_, *gameService_);
}

GameLogRuntime::~GameLogRuntime() = default;

bool GameLogRuntime::start()
{
    if (running_)
    {
        qCWarning(gamelogAgentLog) << "Attempted to start an already-running GameLog runtime.";
        return false;
    }

    if (!databaseReady_ || !gameService_ || !sessionService_)
    {
        qCWarning(gamelogAgentLog) << "Cannot start because application services are unavailable.";
        return false;
    }

    processSource_ = std::make_unique<core::process::ProcfsProcessSource>();

    if (!syncGamesWithDatabase())
    {
        qCWarning(gamelogAgentLog) << "Failed to sync games with the database.";
        processSource_.reset();
        return false;
    }

    if (!restoreActiveSession())
    {
        processSource_.reset();
        return false;
    }

    running_ = true;
    qCInfo(gamelogAgentLog) << "GameLog runtime started";
    qCInfo(gamelogAgentLog) << "Database is:" << (databaseManager_.isOpen() ? "open" : "closed");
    qCInfo(gamelogAgentLog) << "Database path:" << databaseManager_.database().databaseName();
    return true;
}

void GameLogRuntime::stop()
{
    if (!running_)
    {
        return;
    }

    running_ = false;
    processSource_.reset();
    resetPendingStart();
    gameClosedDuration_ = seconds::zero();
    activeGame_.reset();

    // Preserve an active row for headless/GUI launcher handoff.
    qCInfo(gamelogAgentLog) << "GameLog runtime stopped";
}

void GameLogRuntime::update(seconds elapsed)
{
    if (!running_)
    {
        qCWarning(gamelogAgentLog) << "Attempted to update a runtime that is not running.";
        return;
    }
    if (!processSource_)
    {
        qCWarning(gamelogAgentLog) << "Process source is unavailable.";
        return;
    }
    if (elapsed <= seconds::zero())
    {
        qCWarning(gamelogAgentLog) << "Runtime update received a non-positive elapsed duration.";
        return;
    }

    std::vector<ProcessInfo> processes = processSource_->listProcesses();
    if (!trackedSteamGames_.isEmpty())
    {
        steamProcessInspector_.annotate(processes);
    }

    if (!activeGame_)
    {
        // Check if detected game matches a previously detected game.
        std::optional<Game> detectedGame;
        for (const ProcessInfo &process : processes)
        {
            detectedGame = matchTrackedGame(process);
            if (detectedGame)
            {
                break;
            }
        }

        if (!detectedGame)
        {
            resetPendingStart();
            return;
        }

        if (!pendingGameId_ || *pendingGameId_ != detectedGame->id)
        {
            pendingGameId_ = detectedGame->id;
            gameOpenDuration_ = seconds::zero();
        }

        gameOpenDuration_ += elapsed;
        if (gameOpenDuration_ < kStartGracePeriod)
        {
            return;
        }

        static_cast<void>(startNewSession(*detectedGame));
        resetPendingStart();
        return;
    }

    const bool activeGameFound = std::ranges::any_of(
        processes,
        [this](const ProcessInfo &process) {
            return processMatchesGame(process, *activeGame_);
        });

    if (activeGameFound)
    {
        gameClosedDuration_ = seconds::zero();
        return;
    }

    gameClosedDuration_ += elapsed;
    if (gameClosedDuration_ >= kEndGracePeriod)
    {
        static_cast<void>(stopActiveSession());
    }
}

std::optional<d::Session> GameLogRuntime::activeSession() const
{
    if (!sessionService_)
    {
        return std::nullopt;
    }
    return sessionService_->findActiveSession();
}

bool GameLogRuntime::reloadTrackedGames()
{
    return syncGamesWithDatabase();
}

bool GameLogRuntime::syncGamesWithDatabase()
{
    if (!databaseManager_.isOpen() || !gameService_)
    {
        qCWarning(gamelogAgentLog)
            << "Cannot sync games because GameService or the database is unavailable.";
        return false;
    }

    trackedSteamGames_.clear();
    trackedPathGames_.clear();

    for (const Game &game : gameService_->listTrackedGames())
    {
        if (game.steamAppId && *game.steamAppId > 0)
        {
            trackedSteamGames_.insert(
                static_cast<std::uint32_t>(*game.steamAppId), game);
        }
        if (!game.executablePath.isEmpty())
        {
            trackedPathGames_.insert(game.executablePath, game);
        }
    }

    qCInfo(gamelogAgentLog)
        << "Synced" << trackedSteamGames_.size() << "Steam games and"
        << trackedPathGames_.size() << "path-based games.";
    return true;
}

bool GameLogRuntime::restoreActiveSession()
{
    if (!sessionService_ || !gameService_)
    {
        return false;
    }

    const std::optional<Session> session =
        sessionService_->findActiveSession();
    if (!session)
    {
        activeGame_.reset();
        return true;
    }

    const std::optional<Game> game = gameService_->findById(session->gameId);
    if (!game)
    {
        qCWarning(gamelogAgentLog)
            << "Active session" << session->id
            << "references missing game" << session->gameId;
        return false;
    }

    activeGame_ = *game;
    gameClosedDuration_ = seconds::zero();
    qCInfo(gamelogAgentLog)
        << "Restored active session" << session->id << "for game:" << game->title;
    return true;
}

optional<Game> GameLogRuntime::matchTrackedGame(const ProcessInfo &process) const
{
    if (process.steamAppId)
    {
        const auto steamGame = trackedSteamGames_.constFind(*process.steamAppId);
        if (steamGame != trackedSteamGames_.constEnd())
        {
            return steamGame.value();
        }
    }

    if (!process.executablePath.isEmpty())
    {
        const auto pathGame = trackedPathGames_.constFind(process.executablePath);
        if (pathGame != trackedPathGames_.constEnd())
        {
            return pathGame.value();
        }
    }
    return std::nullopt;
}

bool GameLogRuntime::processMatchesGame(const ProcessInfo &process, const Game &game)
{
    if (game.steamAppId && *game.steamAppId > 0 && process.steamAppId)
    {
        return *process.steamAppId ==
            static_cast<std::uint32_t>(*game.steamAppId);
    }
    return !game.executablePath.isEmpty()
        && process.executablePath == game.executablePath;
}

bool GameLogRuntime::startNewSession(const d::Game &game)
{
    if (!sessionService_)
    {
        qCWarning(gamelogAgentLog)
            << "Cannot start a session because SessionService is unavailable.";
        return false;
    }

    const auto session = sessionService_->startAutomaticSession(game.id);
    if (!session)
    {
        qCWarning(gamelogAgentLog)
            << "Failed to start session for:" << game.title;
        return false;
    }

    activeGame_ = game;
    gameClosedDuration_ = seconds::zero();
    qCInfo(gamelogAgentLog)
        << "Started session" << session->id << "for game:" << game.title;
    return true;
}

bool GameLogRuntime::stopActiveSession()
{
    if (!sessionService_ || !activeGame_)
    {
        qCWarning(gamelogAgentLog)
            << "Cannot stop the session because SessionService or the active game is unavailable.";
        return false;
    }

    const QString gameTitle = activeGame_->title;
    const auto endedSession = sessionService_->endActiveSession();
    if (!endedSession)
    {
        qCWarning(gamelogAgentLog)
            << "Failed to complete and persist the active session for:"
            << gameTitle;
        return false;
    }

    activeGame_.reset();
    gameClosedDuration_ = seconds::zero();
    qCInfo(gamelogAgentLog)
        << "Stopped session" << endedSession->id << "for game:" << gameTitle;
    return true;
}

void GameLogRuntime::resetPendingStart() noexcept
{
    pendingGameId_.reset();
    gameOpenDuration_ = seconds::zero();
}

services::GameService * GameLogRuntime::getGameService()
{
    return &gameService_.value();
}

services::SessionService * GameLogRuntime::getSessionService()
{
    return &sessionService_.value();
}

} // namespace gamelog::application
