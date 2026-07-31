//
// Created by kj on 7/31/26.
//

#include "AgentIpcServer.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <vector>

#include "logging/LoggingCategories.h"

using gamelog::agent::AgentIpcServer;

AgentIpcServer::AgentIpcServer() :
QObject(nullptr), m_server(new QLocalServer())
{
    connect(m_server, &QLocalServer::newConnection, this, &AgentIpcServer::handleNewConnection);
}

AgentIpcServer::~AgentIpcServer()
{
    m_server->close();
    delete m_server;
}

bool AgentIpcServer::start(const QString& serverName)
{
    bool success = m_server->listen(serverName);
    if (!success)
    {
        qWarning(gamelogIpcLog) << "Server listen failed:" << m_server;
    }
    return success;
}

void AgentIpcServer::stop()
{
    m_server->close();
}

void AgentIpcServer::handleNewConnection()
{
    std::vector<QLocalSocket*> sockets;
    while (m_server->hasPendingConnections()) {
        QLocalSocket* connection = m_server->nextPendingConnection();
        if (connection) {
            connect(connection, &QLocalSocket::readyRead, this, [this, connection]() {
                handleDataReceived(connection);
            });
            connect(connection, &QLocalSocket::disconnected, this, [this, connection]() {
                handleSocketDisconnected(connection);
            });
            connect(connection, &QLocalSocket::errorOccurred, this, [this, connection]() {
                handleSocketError(connection);
            });
        }
    }
}

void AgentIpcServer::handleDataReceived(QLocalSocket* socket)
{
    if (socket) {
        QByteArray data = socket->readAll();
        QString recievedString(data);
        qDebug(gamelogIpcLog) << "Received data:" << recievedString;
        QString response("pong\n");
        qDebug(gamelogIpcLog) << "Sending back:" << response;
        qint64 code = socket->write(response.toUtf8());
        socket->flush();
        qDebug(gamelogIpcLog) << "Socket emited: " << code << "after writing";
    }
}

void AgentIpcServer::handleSocketDisconnected(QLocalSocket* socket)
{
    if (socket) {
        qInfo(gamelogIpcLog) << "Socket disconnected:" << socket;
    }
    socket->close();
}

void AgentIpcServer::handleSocketError(QLocalSocket* socket)
{
    if (socket) {
        qWarning(gamelogIpcLog) << "Socket error:" << socket << ":" << socket->errorString();
    }
    socket->close();
}
