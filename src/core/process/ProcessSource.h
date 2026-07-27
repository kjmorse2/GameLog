#pragma once

#include <vector>

#include "process/ProcessInfo.h"

namespace gamelog::core::process
{
class ProcessSource
{
public:
    virtual ~ProcessSource() = default;
    [[nodiscard]] virtual std::vector<ProcessInfo> listProcesses() = 0;
};
} // namespace gamelog::core::process
