#include "process/ProcfsProcessSource.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <utility>

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
        // Ask libproc2 for only the fields we actually need for detection.
        std::array requestedItems{PIDS_ID_PID, PIDS_CMD, PIDS_EXE};
        pids_info* info = nullptr;
        const int creationResult = procps_pids_new(&info, requestedItems.data(), requestedItems.size());

        if (creationResult < 0)
        {
            qCritical(gamelogCoreLog) << "Failed to create pids_info structure:" << std::strerror(errno);
            return {};
        }

        pids_fetch* fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);

        if (fetched == nullptr)
        {
            const int savedErrno = errno;
            procps_pids_unref(&info);
            qCritical(gamelogCoreLog) << "Failed to fetch process information:" << std::strerror(savedErrno);
            return {};
        }

        std::vector<ProcessInfo> processes;

        const int totalProcesses = (fetched->counts != nullptr) ? fetched->counts->total : 0;

        if (totalProcesses > 0)
        {
            // Reserve based on the reported total to avoid repeated growth.
            processes.reserve(static_cast<std::size_t>(totalProcesses));
        }

        // Walk each stack entry, skipping any partial rows that libproc2 could not
        // fill.
        for (int index = 0; index < totalProcesses; ++index)
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
            // qInfo(gamelogCoreLog) << "Detected process:" << process.executableName << "PID:" << process.pid << "Path:" << process.executablePath;
            processes.push_back(std::move(process));
        }
        procps_pids_unref(&info);
        return processes;
    }
} // namespace gamelog::core::process
