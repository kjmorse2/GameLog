#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <QHash>
#include <QString>

#include "process/ProcessInfo.h"

namespace gamelog::core::process
{
    class SteamProcessInspector
    {
    public:
        using SteamAppIdReader = std::function<std::optional<std::uint32_t>(qint64)>;

        /**
         * Creates an inspector that reads SteamAppId from the Linux process
         * environment through ProcessHelpers.
         */
        SteamProcessInspector();

        /**
         * Creates an inspector with a caller-provided environment reader.
         * This keeps cache behavior deterministic in tests without changing
         * the production /proc implementation.
         * @param steamAppIdReader Function used to read one PID's Steam App ID.
         */
        explicit SteamProcessInspector(SteamAppIdReader steamAppIdReader);

        /**
         * Adds Steam App IDs to the supplied process snapshot.
         *
         * Environment information is read only for new PIDs or when the
         * executable path associated with a cached PID changes. Entries for
         * PIDs absent from the current snapshot are discarded. SteamAppId is
         * treated as immutable for the cached process lifetime.
         */
        void annotate(std::vector<ProcessInfo>& processes);

    private:
        /**
         * @brief Cached environment result for one observed PID.
         */
        struct CacheEntry
        {
            QString executablePath;
            std::optional<std::uint32_t> steamAppId;
        };

        SteamAppIdReader steamAppIdReader_;
        QHash<qint64, CacheEntry> cache_;
    };
} // namespace gamelog::core::process
