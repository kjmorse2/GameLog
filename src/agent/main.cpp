#include <QCoreApplication>

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

    agentApplication.start();
    return app.exec();
}
