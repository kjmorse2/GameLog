#pragma once

#include <vector>

#include "process/ProcessInfo.h"

namespace gamelog::core::process
{
/**
 * @brief The ProcessSource class is an abstract base class that defines an interface for retrieving information about running processes.
 */
class ProcessSource
{
public:
    /**
     * @brief Virtual destructor for the ProcessSource class.
     */
    virtual ~ProcessSource() = default;

    /**
     * @brief Lists all running processes.
     * @return A vector of ProcessInfo objects representing the running processes.
     */
    [[nodiscard]] virtual std::vector<ProcessInfo> listProcesses() = 0;
};
} // namespace gamelog::core::process
