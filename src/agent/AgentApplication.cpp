#include "AgentApplication.h"

#include "logging/LoggingCategories.h"
#include "process/ProcessSource.h"
#include "process/ProcfsProcessSource.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <QSqlDatabase>

using gamelog::core::process::ProcessInfo;
using gamelog::core::process::ProcessSource;
namespace d = gamelog::core::domain;
using std::chrono::seconds;

namespace gamelog::agent {

    AgentApplication::AgentApplication(QString databasePath) :
        m_databaseManager{std::move(databasePath),
                          QStringLiteral("GameLogAgentConnection")}
    {
        m_databaseReady = m_databaseManager.initialize();

        if (!m_databaseReady)
        {
            qCWarning(gamelogAgentLog) << "Failed to initialize the database manager.";
            return;
        }

        const QSqlDatabase database = m_databaseManager.database();

        // Construct the database interfaces using emplace, as required by std::optinal
        m_gameRepository.emplace(database);
        m_sessionRepository.emplace(database);
        m_sessionManager.emplace(*m_gameRepository, *m_sessionRepository);
    }

    AgentApplication::~AgentApplication() = default;

    bool AgentApplication::start()
    {
        // Check that all flags are green to start listenting and recording.
        if (m_running)
        {
            qCWarning(gamelogAgentLog) << "Attempted to start an already-running agent.";
            return false;
        }

        if (!m_databaseReady || !m_gameRepository || !m_sessionRepository || !m_sessionManager)
        {
            qCWarning(gamelogAgentLog) << "Cannot start the agent because database services are unavailable.";
            return false;
        }

        // Construct process source when ready
        m_processSource = std::make_unique<core::process::ProcfsProcessSource>();

        // Sync games into memory
        if (!syncGamesWithDatabase())
        {
            qCWarning(gamelogAgentLog) << "Failed to sync games with the database.";
            m_processSource.reset();
            return false;
        }

        m_running = true;

        qCInfo(gamelogAgentLog) << "GameLog agent started";
        qCInfo(gamelogAgentLog) << "Database is:" << (m_databaseManager.isOpen() ? "open" : "closed");
        qCInfo(gamelogAgentLog) << "Database path:" << m_databaseManager.database().databaseName();
        return true;
    }

    void AgentApplication::stop()
    {
        if (!m_running)
        {
            return;
        }

        m_running = false;
        m_processSource.reset();
        resetPendingStart();
        m_gameClosedDuration = seconds::zero();

        qCInfo(gamelogAgentLog) << "GameLog agent stopped";
    }

    void AgentApplication::updateAgent(seconds elapsed)
    {
        if (!m_running)
        {
            qCWarning(gamelogAgentLog) << "Attempted to update an agent that is not running.";
            return;
        }

        if (!m_processSource)
        {
            qCWarning(gamelogAgentLog) << "Process source is unavailable.";
            return;
        }

        if (elapsed <= seconds::zero())
        {
            qCWarning(gamelogAgentLog) << "Agent update received a non-positive elapsed duration.";
            return;
        }

        const std::vector<ProcessInfo> processes = m_processSource->listProcesses();

        if (!m_activeGame)
        {
            const auto detectedProcess = std::ranges::find_if(
                    processes,
                    [this](const ProcessInfo &process) {
                        return m_trackedExecutablePaths.contains(process.executablePath);
                    });

            if (detectedProcess == processes.end())
            {
                resetPendingStart();
                return;
            }

            // A different tracked process must earn its own complete grace period.
            if (!m_pendingExecutablePath || *m_pendingExecutablePath != detectedProcess->executablePath)
            {
                m_pendingExecutablePath = detectedProcess->executablePath;
                m_gameOpenDuration = seconds::zero();
            }

            m_gameOpenDuration += elapsed;

            if (m_gameOpenDuration < kStartGracePeriod)
            {
                return;
            }

            // Whether the start succeeds or fails, begin a fresh grace interval
            // before trying this process again. This avoids retry log spam.
            static_cast<void>(startNewSession(*detectedProcess));
            resetPendingStart();
            return;
        }

        const bool activeGameFound = std::ranges::any_of(
                processes,
                [this](const ProcessInfo &process) {
                    return process.executablePath == m_activeGame->executablePath;
                });

        if (activeGameFound)
        {
            m_gameClosedDuration = seconds::zero();
            return;
        }

        m_gameClosedDuration += elapsed;

        if (m_gameClosedDuration >= kEndGracePeriod)
        {
            // On failure, SessionManager retains its active state and this agent
            // retains m_activeGame, so the next poll can safely retry the update.
            static_cast<void>(stopActiveSession());
        }
    }

    bool AgentApplication::syncGamesWithDatabase()
    {
        if (!m_databaseManager.isOpen())
        {
            qCWarning(gamelogAgentLog) << "Cannot sync games because the database is not open.";
            return false;
        }

        if (!m_gameRepository)
        {
            qCWarning(gamelogAgentLog) << "Cannot sync games because the game repository is unavailable.";
            return false;
        }

        m_trackedExecutablePaths.clear();

        for (const d::Game &game: m_gameRepository->findAll())
        {
            if (game.trackingEnabled && !game.executablePath.isEmpty())
            {
                m_trackedExecutablePaths.insert(game.executablePath);
            }
        }

        qCInfo(gamelogAgentLog) << "Synced" << m_trackedExecutablePaths.size() << "tracked executable paths from the database.";
        return true;
    }

    bool AgentApplication::startNewSession(
            const ProcessInfo &detectedProcess)
    {
        if (!m_gameRepository || !m_sessionManager)
        {
            qCWarning(gamelogAgentLog) << "Cannot start a session because database services are unavailable.";
            return false;
        }

        const std::optional<d::Game> game = m_gameRepository->findByPath(detectedProcess.executablePath);

        if (!game)
        {
            qCWarning(gamelogAgentLog) << "Detected executable was not found in the database:" << detectedProcess.executablePath;
            return false;
        }

        const std::optional<d::Session> session = m_sessionManager->startAutomaticSession(game->id);

        if (!session)
        {
            qCWarning(gamelogAgentLog) << "Failed to create and persist a session for:" << game->title;
            return false;
        }

        m_activeGame = *game;
        m_gameClosedDuration = seconds::zero();

        qCInfo(gamelogAgentLog) << "Started session" << session->id << "for game:" << game->title;
        return true;
    }

    bool AgentApplication::stopActiveSession()
    {
        if (!m_sessionManager)
        {
            qCWarning(gamelogAgentLog) << "Cannot stop the session because SessionManager is unavailable.";
            return false;
        }

        if (!m_activeGame)
        {
            qCWarning(gamelogAgentLog) << "Cannot stop the session because the agent has no active game.";
            return false;
        }

        const QString gameTitle = m_activeGame->title;
        const std::optional<d::Session> endedSession = m_sessionManager->endActiveSession();

        if (!endedSession)
        {
            qCWarning(gamelogAgentLog) << "Failed to complete and persist the active session for:" << gameTitle;
            return false;
        }

        m_activeGame.reset();
        m_gameClosedDuration = seconds::zero();

        qCInfo(gamelogAgentLog) << "Stopped session" << endedSession->id << "for game:" << gameTitle;
        return true;
    }

    void AgentApplication::resetPendingStart() noexcept
    {
        m_pendingExecutablePath.reset();
        m_gameOpenDuration = seconds::zero();
    }

} // namespace gamelog::agent
