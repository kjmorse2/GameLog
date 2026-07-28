#pragma once

#include "process/ProcessSource.h"
#include <libproc2/pids.h>
#include <QLoggingCategory>

/**
 * @brief The ProcfsProcessSource class is an implementation of the ProcessSource interface that retrieves information about running processes from the /proc filesystem.
 */
namespace gamelog::core::process
{
/**
 * @brief A class that retrieves information about running processes from the /proc filesystem.
 */
class ProcfsProcessSource final : public ProcessSource
{
public:
    /**
     * @brief Lists all running processes by reading from the /proc filesystem.
     * @return A vector of ProcessInfo objects representing the running processes.
     */
    [[nodiscard]] std::vector<ProcessInfo> listProcesses() override;
};
} // namespace gamelog::core::process
