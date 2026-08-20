#include "application/GameLogRuntime.h"

#include "logging/LoggingCategories.h"
#include "process/ProcessHelpers.h"
#include "process/ProcessSource.h"
#include "process/ProcfsProcessSource.h"

#include <utility>
#include <vector>

#include <QSqlDatabase>

using gamelog::core::process::ProcessInfo;
using std::chrono::seconds;

namespace gamelog::application
{
    namespace
    {
        GameLogRuntime::ProcessSourceFactory defaultProcessSourceFactory()
        {
            return [] { return std::make_unique<core::process::ProcfsProcessSource>(); };
        }

        core::process::SteamProcessInspector::SteamAppIdReader defaultSteamAppIdReader()
        {
            return [](qint64 pid) { return core::process::ProcessHelpers::readSteamAppId(pid); };
        }
    } // namespace

    GameLogRuntime::GameLogRuntime(QString databasePath)
        : GameLogRuntime{std::move(databasePath), defaultProcessSourceFactory(), defaultSteamAppIdReader()} {}

    GameLogRuntime::GameLogRuntime(QString databasePath,
                                   ProcessSourceFactory processSourceFactory,
                                   core::process::SteamProcessInspector::SteamAppIdReader steamAppIdReader)
        : databaseManager_{std::move(databasePath), QStringLiteral("GameLogRuntimeConnection")},
          processSourceFactory_{std::move(processSourceFactory)},
          steamProcessInspector_{std::move(steamAppIdReader)}
    {
        databaseReady_ = databaseManager_.initialize();
        if(!databaseReady_)
        {
            qCWarning(gamelogRuntimeLog) << "Failed to initialize the database manager.";
            return;
        }

        const QSqlDatabase database = databaseManager_.database();
        gameRepository_.emplace(database);
        sessionRepository_.emplace(database);
        credentialService_.emplace();
        credentialService_->removeAllSecrets();
        steamApiService_.emplace(*credentialService_);
        gameService_.emplace(*gameRepository_, *steamApiService_);
        sessionService_.emplace(*sessionRepository_, *gameService_);
        gameArtworkService_.emplace();

        connect(&*gameArtworkService_,
                &services::GameArtworkService::artworkAvailable,
                &*gameService_,
                [this](int gameId, services::ArtworkType artworkType)
                {
                    // hasArtwork currently means a usable cover.jpg exists.
                    if(artworkType == services::ArtworkType::Cover && !gameService_->setHasArtwork(gameId, true))
                    {
                        qCWarning(gamelogRuntimeLog) << "Failed to record available cover artwork for game" << gameId;
                    }
                });

        connect(&*gameArtworkService_,
                &services::GameArtworkService::artworkUnavailable,
                &*gameService_,
                [this](int gameId, services::ArtworkType artworkType)
                {
                    if(artworkType == services::ArtworkType::Cover && !gameService_->setHasArtwork(gameId, false))
                    {
                        qCWarning(gamelogRuntimeLog) << "Failed to clear unavailable cover artwork for game" << gameId;
                    }
                });

        connect(&*gameService_,
                &services::GameService::gameAdded,
                &*gameArtworkService_,
                &services::GameArtworkService::getGameArtwork);
    }

    GameLogRuntime::~GameLogRuntime() = default;

    bool GameLogRuntime::start()
    {
        if(running_)
        {
            qCWarning(gamelogRuntimeLog) << "Attempted to start an already-running GameLog runtime.";
            return false;
        }

        if(!databaseReady_ || !gameService_ || !sessionService_ || !processSourceFactory_)
        {
            qCWarning(gamelogRuntimeLog) << "Cannot start because application services are unavailable.";
            return false;
        }

        processSource_ = processSourceFactory_();
        if(!processSource_)
        {
            qCWarning(gamelogRuntimeLog) << "Cannot start because the process source factory returned null.";
            return false;
        }

        // The services own their state; the runtime only asks them to refresh/restore it.
        gameService_->syncGamesWithDatabase();
        if(!sessionService_->restoreActiveSession())
        {
            processSource_.reset();
            return false;
        }

        running_ = true;
        qCInfo(gamelogRuntimeLog) << "GameLog runtime started";
        qCInfo(gamelogRuntimeLog) << "Database is:" << (databaseManager_.isOpen() ? "open" : "closed");
        qCInfo(gamelogRuntimeLog) << "Database path:" << databaseManager_.database().databaseName();
        return true;
    }

    void GameLogRuntime::stop()
    {
        if(!running_) { return; }

        running_ = false;
        processSource_.reset();

        if(sessionService_)
        {
            // Preserve the active database row for launcher handoff, but discard
            // process-polling timers and the cached active-game association.
            sessionService_->resetAutomaticTracking();
        }

        qCInfo(gamelogRuntimeLog) << "GameLog runtime stopped";
    }

    void GameLogRuntime::update(seconds elapsed)
    {
        if(!running_)
        {
            qCWarning(gamelogRuntimeLog) << "Attempted to update a runtime that is not running.";
            return;
        }
        if(!processSource_ || !gameService_ || !sessionService_)
        {
            qCWarning(gamelogRuntimeLog) << "Runtime process tracking dependencies are unavailable.";
            return;
        }
        if(elapsed <= seconds::zero())
        {
            qCWarning(gamelogRuntimeLog) << "Runtime update received a non-positive elapsed duration.";
            return;
        }

        std::vector<ProcessInfo> processes = processSource_->listProcesses();

        if(gameService_->hasTrackedSteamGames()) { steamProcessInspector_.annotate(processes); }

        sessionService_->updateAutomaticTracking(processes, elapsed);
    }

    services::GameService* GameLogRuntime::getGameService() noexcept { return gameService_ ? &*gameService_ : nullptr; }

    services::SessionService* GameLogRuntime::getSessionService() noexcept
    {
        return sessionService_ ? &*sessionService_ : nullptr;
    }

    services::GameArtworkService* GameLogRuntime::getArtworkService() noexcept
    {
        return gameArtworkService_ ? &*gameArtworkService_ : nullptr;
    }

    services::CredentialService* GameLogRuntime::getCredentialService() noexcept
    {
        return credentialService_ ? &*credentialService_ : nullptr;
    }
} // namespace gamelog::application
