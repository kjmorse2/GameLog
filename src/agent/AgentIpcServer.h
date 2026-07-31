//
// Created by kj on 7/31/26.
//
#pragma once

#include "QLocalServer"

#ifndef GAMELOG_AGENTIPCSERVER_H
#define GAMELOG_AGENTIPCSERVER_H

#include <QObject>

namespace gamelog::agent {
    class AgentIpcServer : public QObject
    {
        Q_OBJECT
    public:
        AgentIpcServer();
        ~AgentIpcServer() override;
        bool start(const QString& serverName);
        void stop();


    private slots:
        void handleNewConnection();
        void handleDataReceived(QLocalSocket* socket);
        void handleSocketDisconnected(QLocalSocket* socket);
        void handleSocketError(QLocalSocket* socket);

    private:
        QLocalServer *m_server;
    };
}


#endif // GAMELOG_AGENTIPCSERVER_H
