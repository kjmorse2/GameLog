#pragma once

#include <cstdint>
#include <optional>

#include <QString>
#include <QtTypes>

namespace gamelog::core::process
{
    /**
     * One running process as seen by a ProcessSource, reduced to the fields
     * game matching needs.
     *
     * A plain snapshot value: it is copied freely and never refers back to the
     * live process, so a ProcessInfo may describe a process that has since
     * exited. steamAppId is populated by SteamProcessInspector, not by the
     * process source, and is absent for non-Steam processes and for processes
     * whose environment could not be read.
     */
    struct ProcessInfo
    {
        /**
         * @brief pid of process
         */
        qint64 pid{0};

        /**
         * @brief Name of executable.
         */
        QString executableName;

        /**
         * @brief Full path to executable.
         */
        QString executablePath;

        /**
         * @brief The Steam App ID of the process if found.
         */
        std::optional<std::uint32_t> steamAppId;
    };
} // namespace gamelog::core::process
