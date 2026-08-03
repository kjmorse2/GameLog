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
    : databaseManager_{std::move(databasePath),
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
    sessionManager_.emplace(*gameRepository_, *sessionRepository_);
}

GameLogRuntime::~GameLogRuntime() = default;

bool GameLogRuntime::start()
{
    if (running_)
    {
        qCWarning(gamelogAgentLog) << "Attempted to start an already-running GameLog runtime.";
        return false;
    }

    if (!databaseReady_ || !gameRepository_ || !sessionRepository_ || !sessionManager_)
    {
        qCWarning(gamelogAgentLog) << "Cannot start because database services are unavailable.";
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

    // Do not complete an active session here. During a headless <-> GUI
    // handoff the next process restores the same active row from the database.
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
        std::optional<d::Game> detectedGame;

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

std::vector<d::Game> GameLogRuntime::listGames() const
{
    if (!gameRepository_)
    {
        qCWarning(gamelogAgentLog)
            << "Cannot list games because the repository is unavailable.";
        return {};
    }

    return gameRepository_->findAll();
}

std::optional<d::Session> GameLogRuntime::activeSession() const
{
    if (!sessionManager_)
    {
        return std::nullopt;
    }

    return sessionManager_->activeSession();
}

bool GameLogRuntime::reloadTrackedGames()
{
    return syncGamesWithDatabase();
}

bool GameLogRuntime::syncGamesWithDatabase()
{
    if (!databaseManager_.isOpen())
    {
        qCWarning(gamelogAgentLog)
            << "Cannot sync games because the database is not open.";
        return false;
    }

    if (!gameRepository_)
    {
        qCWarning(gamelogAgentLog)
            << "Cannot sync games because the game repository is unavailable.";
        return false;
    }

    trackedSteamGames_.clear();
    trackedPathGames_.clear();

    for (const d::Game &game : gameRepository_->findAll())
    {
        if (!game.trackingEnabled)
        {
            continue;
        }

        if (game.steamAppId && *game.steamAppId > 0)
        {
            trackedSteamGames_.insert(static_cast<std::uint32_t>(*game.steamAppId), game);
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
    if (!sessionManager_ || !gameRepository_)
    {
        return false;
    }

    const std::optional<d::Session> session = sessionManager_->activeSession();

    if (!session)
    {
        activeGame_.reset();
        return true;
    }

    const std::optional<d::Game> game = gameRepository_->findById(session->gameId);

    if (!game)
    {
        qCWarning(gamelogAgentLog)
            << "Active session" << session->id
            << "references missing game" << session->gameId;
        return false;
    }

    activeGame_ = *game;
    gameClosedDuration_ = seconds::zero();

    qCInfo(gamelogAgentLog) << "Restored active session" << session->id << "for game:" << game->title;

    return true;
}

std::optional<d::Game> GameLogRuntime::matchTrackedGame(const ProcessInfo &process) const
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
        const auto pathGame =
            trackedPathGames_.constFind(process.executablePath);

        if (pathGame != trackedPathGames_.constEnd())
        {
            return pathGame.value();
        }
    }

    return std::nullopt;
}

bool GameLogRuntime::processMatchesGame(
    const ProcessInfo &process,
    const d::Game &game) const
{
    if (game.steamAppId && *game.steamAppId > 0 && process.steamAppId)
    {
        return *process.steamAppId == static_cast<std::uint32_t>(*game.steamAppId);
    }

    return !game.executablePath.isEmpty() &&
           process.executablePath == game.executablePath;
}

bool GameLogRuntime::startNewSession(const d::Game &game)
{
    if (!sessionManager_)
    {
        qCWarning(gamelogAgentLog)
            << "Cannot start a session because SessionManager is unavailable.";
        return false;
    }

    const std::optional<d::Session> session =
        sessionManager_->startAutomaticSession(game.id);

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
    if (!sessionManager_)
    {
        qCWarning(gamelogAgentLog)
            << "Cannot stop the session because SessionManager is unavailable.";
        return false;
    }

    if (!activeGame_)
    {
        qCWarning(gamelogAgentLog)
            << "Cannot stop the session because no active game is known.";
        return false;
    }

    const QString gameTitle = activeGame_->title;
    const std::optional<d::Session> endedSession =
        sessionManager_->endActiveSession();

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
        << "Stopped session" << endedSession->id << "for game:"
        << gameTitle;
    return true;
}

void GameLogRuntime::resetPendingStart() noexcept
{
    pendingGameId_.reset();
    gameOpenDuration_ = seconds::zero();
}

} // namespace gamelog::application
