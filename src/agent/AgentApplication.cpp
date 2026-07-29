#include "AgentApplication.h"

#include "database/GameRepository.h"
#include "logging/LoggingCategories.h"
#include "process/ProcfsProcessSource.h"

#include <memory>
#include <utility>
#include <vector>

#include <QSqlDatabase>
#include <QString>

namespace gamelog::agent {

    AgentApplication::AgentApplication(QString databasePath) :
        m_databaseManager{std::move(databasePath), "GameLogAgentConnection"}
    {
        m_databaseReady = m_databaseManager.initialize();

        if (!m_databaseReady)
        {
            qCWarning(gamelogAgentLog) << "Failed to initialize the database manager.";
            return;
        }

        const QSqlDatabase database = m_databaseManager.database();

        m_gameRepository.emplace(database);
        m_sessionRepository.emplace(database);

        m_sessionManager.emplace(*m_gameRepository, *m_sessionRepository);
    }

    void AgentApplication::start()
    {
        if (m_running)
        {
            qCWarning(gamelogAgentLog) << "Attempted to start an already-running agent.";
            return;
        }

        if (!m_databaseReady)
        {
            qCWarning(gamelogAgentLog) << "Cannot start the agent because the database was not initialized.";
            return;
        }

        // The process source is lightweight, so create it only when the agent starts.
        m_processSource = std::make_unique<core::process::ProcfsProcessSource>();

        if (!syncGamesWithDatabase())
        {
            qCWarning(gamelogAgentLog) << "Failed to sync games with the database.";
        }

        m_running = true;

        qCInfo(gamelogAgentLog) << "GameLog agent started";
        qCInfo(gamelogAgentLog) << "Database is: " << (m_databaseManager.isOpen() ? "open" : "closed");
        qCInfo(gamelogAgentLog) << "Database path: " << m_databaseManager.database().databaseName();
    }

    void AgentApplication::stop()
    {
        if (!m_running)
        {
            return;
        }

        m_running = false;
        m_processSource.reset();

        qCInfo(gamelogAgentLog) << "GameLog agent stopped";
    }

    void AgentApplication::updateAgent(int secondsSinceLastCall)
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

        const auto processes = m_processSource->listProcesses();

        if (!m_recording)
        {
            std::optional<core::process::ProcessInfo> detectedProcess;

            for (const auto& process : processes)
            {
                const std::string path = process.executablePath.toStdString();

                if (m_trackedExecutables.contains(path))
                {
                    detectedProcess = process;
                    break;
                }
            }

            if (!detectedProcess.has_value())
            {
                m_secondsGameHasBeenOpened = 0;
                return;
            }

            m_secondsGameHasBeenOpened += secondsSinceLastCall;

            if (m_secondsGameHasBeenOpened < 30)
            {
                return;
            }

            core::domain::Game detectedGame;
            detectedGame.executablePath = detectedProcess->executablePath;
            detectedGame.executableName = detectedProcess->executableName;
            startNewSession(detectedGame);
            return;
        }

        bool activeGameFound = false;

        for (const auto& process : processes)
        {
            if (process.executablePath == m_activeGame.executablePath)
            {
                activeGameFound = true;
                break;
            }
        }

        if (activeGameFound)
        {
            m_secondsGameHasBeenClosed = 0;
            return;
        }

        m_secondsGameHasBeenClosed += secondsSinceLastCall;

        if (m_secondsGameHasBeenClosed >= 30)
        {
            stopActiveSession();
        }
    }

    bool AgentApplication::syncGamesWithDatabase()
    {
        if (!m_databaseManager.isOpen())
        {
            qCWarning(gamelogAgentLog) << "Cannot sync games because the database is not open.";

            return false;
        }

        // Rebuild the cache from scratch so stale executable paths do not linger.
        m_trackedExecutables.clear();

        if (!m_gameRepository)
        {
            qCWarning(gamelogAgentLog) << "Cannot sync games because the game repository is not available.";
            return false;
        }

        if (!m_sessionRepository)
        {
            qCWarning(gamelogAgentLog) << "Cannot sync games because the session repository is not available.";
            return false;
        }

        const auto games = m_gameRepository->findAll();

        for (const auto &game: games)
        {
            if (!game.executablePath.isEmpty())
            {
                m_trackedExecutables.insert(game.executablePath.toStdString());
            }
        }

        qCInfo(gamelogAgentLog) << "Syncing games with database...";

        return true;
    }

    void AgentApplication::startNewSession(
    const core::domain::Game& foundGame
)
    {
        if (!m_gameRepository || !m_sessionRepository || !m_sessionManager)
        {
            qCWarning(gamelogAgentLog) << "Cannot start a session because database services are unavailable.";
            return;
        }

        const std::optional<core::domain::Game> potentialGame = m_gameRepository->findByPath(foundGame.executablePath);

        if (!potentialGame.has_value())
        {
            qCWarning(gamelogAgentLog) << "Detected executable was not found in the database:" << foundGame.executablePath;
            return;
        }

        const core::domain::Game& fullGame = *potentialGame;

        const std::optional<core::domain::Session>
            potentialSession = m_sessionManager->startAutomaticSession(fullGame.id);

        if (!potentialSession.has_value())
        {
            qCWarning(gamelogAgentLog) << "Failed to create a session for:" << fullGame.title;
            return;
        }

        core::domain::Session newSession = *potentialSession;

        if (!m_sessionRepository->insert(newSession))
        {
            qCWarning(gamelogAgentLog) << "Failed to persist the session for:" << fullGame.title;

            // Reset SessionManager's in-memory active state.
            static_cast<void>(m_sessionManager->endActiveSession());

            return;
        }

        m_activeGame = fullGame;
        m_activeSession = newSession;
        m_recording = true;
        m_secondsGameHasBeenOpened = 0;
        m_secondsGameHasBeenClosed = 0;

        qCInfo(gamelogAgentLog) << "Started session for game:" << fullGame.title;
    }

    void AgentApplication::stopActiveSession()
    {
        if (!m_sessionManager || !m_sessionRepository)
        {
            qCWarning(gamelogAgentLog) << "Cannot stop the session because database services are unavailable.";
            return;
        }

        const std::optional<core::domain::Session> potentialEndedSession = m_sessionManager->endActiveSession();

        if (!potentialEndedSession.has_value())
        {
            qCWarning(gamelogAgentLog) << "Failed to end the active session.";
            return;
        }

        const core::domain::Session& endedSession = *potentialEndedSession;

        if (!m_sessionRepository->update(endedSession))
        {
            qCWarning(gamelogAgentLog) << "Failed to persist the ended session.";
            return;
        }

        m_activeSession = endedSession;
        m_recording = false;
        m_secondsGameHasBeenOpened = 0;
        m_secondsGameHasBeenClosed = 0;

        qCInfo(gamelogAgentLog) << "Stopped session for game:" << m_activeGame.title;
    }
} // namespace gamelog::agent
