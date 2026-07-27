#pragma once

#include <vector>

#include "process/ProcessSource.h"

namespace gamelog::core::process
{
class ProcfsProcessSource final : public ProcessSource
{
public:
    [[nodiscard]] std::vector<ProcessInfo> listProcesses() override
    {
        // TODO: Enumerate processes from /proc and map to ProcessInfo.
        return {};
    }
};
} // namespace gamelog::core::process
