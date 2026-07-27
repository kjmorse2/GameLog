#pragma once

namespace gamelog::core::ipc
{
class IpcServer
{
public:
    IpcServer() = default;

    [[nodiscard]] bool start();
    void stop();
};
} // namespace gamelog::core::ipc
