#pragma once

#include <QString>

namespace gamelog::core::process
{

/**
 * @brief Describes one running process discovered by a source.
 */
struct ProcessInfo
{
    /**
     * @brief Process identifier.
     */
    qint64 pid{0};

    /**
     * @brief Basename or command string reported by the source.
     */
    QString executableName;

    /**
     * @brief Absolute executable path when the source can resolve it.
     */
    QString executablePath;
};
} // namespace gamelog::core::process
