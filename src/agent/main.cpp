#include <QCoreApplication>
#include <QTimer>

#include "AgentApplication.h"
#include "database/DatabaseManager.h"
#include "logging/LoggingCategories.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("GameLog"));
    QCoreApplication::setApplicationName(QStringLiteral("GameLogDev"));

    // Resolve the database location once so the agent and tests use the same
    // file.
    const QString databasePath = gamelog::core::database::DatabaseManager::resolveDatabasePath();

    if (databasePath.isEmpty())
    {
        qCritical(gamelogDatabaseLog) << "Failed to determine database path.";
        return 1;
    }

    gamelog::agent::AgentApplication agentApplication(databasePath);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&agentApplication]() { agentApplication.stop(); });

    // Keep a simple heartbeat so process detection stays responsive.
    QTimer timer;
    timer.setInterval(5000); // Check for games every 5 seconds
    QObject::connect(&timer, &QTimer::timeout, [&agentApplication]() { agentApplication.checkForGames(); });
    timer.start(5000); // Check for games every 5 seconds

    agentApplication.start();
    return app.exec();
}
