#pragma once

#include <QString>

namespace gamelog::core::process
{

/**
 * @brief The ProcessInfo struct represents information about a running process.
 */
struct ProcessInfo
{
    /**
     * @brief The process ID (PID) of the running process.
     */
    qint64 pid{0};

    /**
     * @brief The command used to start the process.
     */
    QString executableName;

    /**
     * @brief The full path to the executable of the process.
     */
    QString executablePath;
};
} // namespace gamelog::core::process
