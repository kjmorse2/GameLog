#pragma once

#include <QByteArray>
#include <QString>

namespace gamelog::core::ipc
{
struct IpcMessage
{
    QString messageType;
    QByteArray payload;
};
} // namespace gamelog::core::ipc
