#pragma once

#include <cstdint>
#include <optional>

#include <QString>
#include <QtTypes>

namespace gamelog::core::process {

    struct ProcessInfo
    {
        qint64 pid{0};

        QString executableName;
        QString executablePath;

        std::optional<std::uint32_t> steamAppId;
    };

} // namespace gamelog::core::process
