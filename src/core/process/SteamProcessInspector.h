#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QHash>
#include <QString>

#include "process/ProcessInfo.h"

namespace gamelog::core::process {

    class SteamProcessInspector
    {
    public:
        /**
         * Adds Steam App IDs to the supplied process snapshot.
         *
         * Environment information is read only for new processes,
         * processes whose executable has changed, or reused PIDs.
         */
        void annotate(std::vector<ProcessInfo> &processes);

    private:
        struct CacheEntry
        {
            QString executablePath;

            std::optional<std::uint32_t>
                    steamAppId;
        };

        QHash<qint64, CacheEntry> cache_;
    };

} // namespace gamelog::core::process
