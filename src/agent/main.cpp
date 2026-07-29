#include <chrono>
#include <cstdlib>

#include <QCoreApplication>
#include <QTimer>

#include "AgentApplication.h"
#include "database/DatabaseManager.h"
#include "logging/LoggingCategories.h"

int main(int argc, char *argv[])
{
    QCoreApplication app{argc, argv};
    QCoreApplication::setOrganizationName(QStringLiteral("GameLog"));
    QCoreApplication::setApplicationName(QStringLiteral("GameLogDev"));

    const QString databasePath =
            gamelog::core::database::DatabaseManager::resolveDatabasePath();

    if (databasePath.isEmpty())
    {
        qCritical(gamelogDatabaseLog) << "Failed to determine database path.";
        return EXIT_FAILURE;
    }

    gamelog::agent::AgentApplication agentApplication{databasePath};

    QObject::connect(
            &app,
            &QCoreApplication::aboutToQuit,
            [&agentApplication] { agentApplication.stop(); });

    constexpr std::chrono::seconds updateInterval{5};

    QTimer timer;
    timer.setInterval(static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(updateInterval)
                    .count()));

    QObject::connect(
            &timer,
            &QTimer::timeout,
            [&agentApplication, updateInterval] {
                agentApplication.updateAgent(updateInterval);
            });

    if (!agentApplication.start())
    {
        qCritical(gamelogAgentLog) << "Failed to start the agent.";
        return EXIT_FAILURE;
    }

    timer.start();
    return app.exec();
}
