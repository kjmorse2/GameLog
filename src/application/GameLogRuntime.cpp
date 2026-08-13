#include "application/GameLogRuntime.h"

#include "logging/LoggingCategories.h"
#include "process/ProcessSource.h"
#include "process/ProcfsProcessSource.h"

#include <utility>
#include <vector>

#include <QSqlDatabase>

using gamelog::core::process::ProcessInfo;
using std::chrono::seconds;

namespace gamelog::application
{
    GameLogRuntime::GameLogRuntime(QString databasePath)
        : databaseManager_{std::move(databasePath), QStringLiteral("GameLogRuntimeConnection")}
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

        // The services own their state; the runtime only asks them to refresh/restore it.
        gameService_->syncGamesWithDatabase();
        if (!sessionService_->restoreActiveSession())
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

        if (sessionService_)
        {
            // Preserve the active database row for launcher handoff, but discard
            // process-polling timers and the cached active-game association.
            sessionService_->resetAutomaticTracking();
        }

        qCInfo(gamelogAgentLog) << "GameLog runtime stopped";
    }

    void GameLogRuntime::update(seconds elapsed)
    {
        if (!running_)
        {
            qCWarning(gamelogAgentLog) << "Attempted to update a runtime that is not running.";
            return;
        }
        if (!processSource_ || !gameService_ || !sessionService_)
        {
            qCWarning(gamelogAgentLog) << "Runtime process tracking dependencies are unavailable.";
            return;
        }
        if (elapsed <= seconds::zero())
        {
            qCWarning(gamelogAgentLog) << "Runtime update received a non-positive elapsed duration.";
            return;
        }

        std::vector<ProcessInfo> processes = processSource_->listProcesses();

        if (gameService_->hasTrackedSteamGames())
        {
            steamProcessInspector_.annotate(processes);
        }

        sessionService_->updateAutomaticTracking(processes, elapsed);
    }

    services::GameService* GameLogRuntime::getGameService() noexcept
    {
        return gameService_ ? &*gameService_ : nullptr;
    }

    services::SessionService* GameLogRuntime::getSessionService() noexcept
    {
        return sessionService_ ? &*sessionService_ : nullptr;
    }
} // namespace gamelog::application
