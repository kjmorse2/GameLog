#pragma once

#include <vector>

#include "process/ProcessInfo.h"

namespace gamelog::core::process
{
/**
 * @brief Abstracts process enumeration so the agent can swap sources later.
 */
class ProcessSource
{
public:
    /**
     * @brief Ensures derived sources clean up correctly through the base type.
     */
    virtual ~ProcessSource() = default;

    /**
     * @brief Lists the currently running processes.
     * @return One ProcessInfo record per observed process.
     */
    [[nodiscard]] virtual std::vector<ProcessInfo> listProcesses() = 0;
};
} // namespace gamelog::core::process
