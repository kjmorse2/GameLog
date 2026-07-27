#pragma once

#include <QString>

namespace gamelog::core::process
{
struct ProcessInfo
{
    qint64 pid{0};
    QString executableName;
    QString executablePath;
};
} // namespace gamelog::core::process
