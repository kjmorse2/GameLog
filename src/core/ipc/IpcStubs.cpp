#include "ipc/IpcClient.h"
#include "ipc/IpcServer.h"

#include <QLoggingCategory>

#include "logging/LoggingCategories.h"

namespace gamelog::core::ipc
{
bool IpcServer::start()
{
    qCInfo(gamelogIpcLog) << "IPC server start is not implemented.";
    // TODO: Use QLocalServer with length-prefixed UTF-8 JSON framing.
    return true;
}

void IpcServer::stop()
{
    qCInfo(gamelogIpcLog) << "IPC server stop is not implemented.";
}

bool IpcClient::connectToServer()
{
    qCInfo(gamelogIpcLog) << "IPC client connect is not implemented.";
    // TODO: Use QLocalSocket transport for local IPC.
    return true;
}

bool IpcClient::sendMessage(const IpcMessage &message)
{
    Q_UNUSED(message);
    qCInfo(gamelogIpcLog) << "IPC client send is not implemented.";
    // TODO: Send length-prefixed UTF-8 JSON protocol messages.
    return false;
}

void IpcClient::disconnectFromServer()
{
    qCInfo(gamelogIpcLog) << "IPC client disconnect is not implemented.";
}
} // namespace gamelog::core::ipc
