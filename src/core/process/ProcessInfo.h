#pragma once

#include <cstdint>
#include <optional>

#include <QString>
#include <QtTypes>

namespace gamelog::core::process
{
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
