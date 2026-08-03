#pragma once

#include <cstdint>
#include <optional>

#include <QByteArray>
#include <QtTypes>

namespace gamelog::core::process {

    class ProcessHelpers
    {
    public:
        /**
         * @brief Reads an envirometn variable for a process with a provided pid
         * @param pid The pid of the process to examine.
         * @param variableName The name of the environment variable to read.
         * @return The value of the environment variable, or std::nullopt if it could not be read.
         */
        [[nodiscard]] static std::optional<QByteArray> readProcessEnvironmentValue(qint64 pid, const QByteArray &variableName);

        /**
         * @brief Reads the Steam App ID for a process with a provided pid.
         * @param pid The pid of the process to examine.
         * @return The Steam App ID, or std::nullopt if it could not be read.
         */
        [[nodiscard]]
        static std::optional<std::uint32_t> readSteamAppId(qint64 pid);
    };

} // namespace gamelog::core::process
