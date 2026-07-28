#include <QCoreApplication>
#include <QTimer>

#include "AgentApplication.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("GameLog"));
    QCoreApplication::setApplicationName(QStringLiteral("gamelog-agent"));

    gamelog::agent::AgentApplication agentApplication;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&agentApplication]() {
        agentApplication.stop();
    });

    QTimer timer;
    timer.setInterval(5000); // Check for games every 5 seconds
    QObject::connect(&timer, &QTimer::timeout, [&agentApplication]() {
        agentApplication.checkForGames();
    });
    timer.start(5000); // Check for games every 5 seconds

    agentApplication.start();
    return app.exec();
}