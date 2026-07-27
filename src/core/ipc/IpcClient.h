#pragma once

#include "ipc/IpcMessage.h"

namespace gamelog::core::ipc
{
class IpcClient
{
public:
    IpcClient() = default;

    [[nodiscard]] bool connectToServer();
    [[nodiscard]] bool sendMessage(const IpcMessage &message);
    void disconnectFromServer();
};
} // namespace gamelog::core::ipc
