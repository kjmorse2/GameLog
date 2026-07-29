#pragma once

#include "process/ProcessSource.h"

namespace gamelog::core::process {
    /**
     * @brief Enumerates processes from the Linux /proc filesystem.
     */
    class ProcfsProcessSource final : public ProcessSource
    {
    public:
        /**
         * @brief Reads the current process table through libproc2.
         */
        [[nodiscard]] std::vector<ProcessInfo> listProcesses() override;
    };
} // namespace gamelog::core::process
