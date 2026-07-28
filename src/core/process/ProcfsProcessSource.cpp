#include "process/ProcfsProcessSource.h"

#include <array>
#include <cerrno>
#include <cstring>
#include "logging/LoggingCategories.h"

#include <libproc2/pids.h>


namespace gamelog::core::process
{
namespace
{

enum ResultIndex
{
    ResultPid,
    ResultCommand,
    ResultExecutable,
    ResultCount
};

} // namespace

std::vector<ProcessInfo> ProcfsProcessSource::listProcesses()
{
    std::array<pids_item, ResultCount> requestedItems{
        PIDS_ID_PID,
        PIDS_CMD,
        PIDS_EXE
    };

    pids_info* info = nullptr;

    const int creationResult = procps_pids_new(
        &info,
        requestedItems.data(),
        static_cast<int>(requestedItems.size())
    );

    if (creationResult < 0)
    {
        qCritical(gamelogCoreLog) << "Failed to create pids_info structure:" << std::strerror(errno);
        return {};
    }

    pids_fetch* fetched = procps_pids_reap(
        info,
        PIDS_FETCH_TASKS_ONLY
    );

    if (fetched == nullptr)
    {
        const int savedErrno = errno;

        procps_pids_unref(&info);

        qCritical(gamelogCoreLog) << "Failed to fetch process information:" << std::strerror(savedErrno);
        return {};
    }

    std::vector<ProcessInfo> processes;

    if (fetched->counts != nullptr)
    {
        processes.reserve(
            static_cast<std::size_t>(fetched->counts->total)
        );
    }

    for (int index = 0; index < fetched->counts->total; ++index)
    {
        pids_stack* stack = fetched->stacks[index];

        if (stack == nullptr)
        {
            continue;
        }

        const int pid = PIDS_VAL(ResultPid, s_int, stack);

        const char* command = PIDS_VAL(ResultCommand, str, stack);

        const char* executable = PIDS_VAL(ResultExecutable, str, stack);

        ProcessInfo process;
        process.pid = static_cast<qint64>(pid);

        if (command != nullptr)
        {
            process.executableName = QString::fromLocal8Bit(command);
        }

        if (executable != nullptr)
        {
            process.executablePath = QString::fromLocal8Bit(executable);
        }

        processes.push_back(std::move(process));
    }

    procps_pids_unref(&info);

    return processes;
}

} // namespace gamelog::core::process