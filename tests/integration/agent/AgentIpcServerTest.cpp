#include <QLocalSocket>
#include <QtTest/QtTest>
#include "AgentIpcServer.h"

namespace {
} // namespace
class AgentIpcServerTest: public QObject
{
    Q_OBJECT

private slots:
    void respondsToCheckWithReady();
};

void AgentIpcServerTest::respondsToCheckWithReady()
{
    QString serverName("Test1");
    gamelog::agent::AgentIpcServer agent{};

    QVERIFY2(agent.start(serverName), "Agent failed to start");
    auto* socket = new QLocalSocket();

    QSignalSpy connectSpy(socket, &QLocalSocket::connected);
    QSignalSpy recieveSpy(socket, &QLocalSocket::readyRead);
    socket->connectToServer(serverName);

    QVERIFY2(connectSpy.count() > 0 || connectSpy.wait(10000), "Failed to connect to the agent");
    socket->write("ping\n");
    QVERIFY2(recieveSpy.count() > 0 || recieveSpy.wait(10000), "Failed to receive data from the agent");
    QByteArray recieved = socket->readAll();
    QVERIFY2(QString(recieved) == QString("pong\n"), "Expected 'pong', got something else");

    socket->disconnectFromServer();
    delete socket;
    agent.stop();
}

QTEST_GUILESS_MAIN(AgentIpcServerTest)

#include "AgentIpcServerTest.moc"