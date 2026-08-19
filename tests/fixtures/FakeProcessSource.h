#pragma once

#include <utility>
#include <vector>

#include "process/ProcessInfo.h"
#include "process/ProcessSource.h"

namespace gamelog::tests::fixtures
{
    /**
     * @brief A ProcessSource that returns a caller-supplied snapshot.
     *
     * Higher-level lifecycle tests need deterministic process listings rather
     * than whatever happens to be running on the machine. This implements the
     * existing pure-virtual seam and needs no state beyond the snapshot, so it
     * stays header-only.
     */
    class FakeProcessSource final : public core::process::ProcessSource
    {
    public:
        FakeProcessSource() = default;

        explicit FakeProcessSource(std::vector<core::process::ProcessInfo> processes)
            : processes_{std::move(processes)} {}

        [[nodiscard]] std::vector<core::process::ProcessInfo> listProcesses() override
        {
            ++listProcessesCallCount_;
            return processes_;
        }

        void setProcesses(std::vector<core::process::ProcessInfo> processes) { processes_ = std::move(processes); }

        [[nodiscard]] int listProcessesCallCount() const noexcept { return listProcessesCallCount_; }

    private:
        std::vector<core::process::ProcessInfo> processes_;
        int listProcessesCallCount_{0};
    };
} // namespace gamelog::tests::fixtures
