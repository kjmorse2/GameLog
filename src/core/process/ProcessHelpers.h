#pragma once

#include <cstdint>
#include <optional>

#include <QByteArray>
#include <QtTypes>

namespace gamelog::core::process {

    class ProcessHelpers
    {
    public:
        [[nodiscard]]
        static std::optional<QByteArray> readProcessEnvironmentValue(qint64 pid, const QByteArray &variableName);

        [[nodiscard]]
        static std::optional<std::uint32_t> readSteamAppId(qint64 pid);
    };

} // namespace gamelog::core::process
