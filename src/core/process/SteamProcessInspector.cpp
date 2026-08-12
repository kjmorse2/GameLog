#include "process/SteamProcessInspector.h"

#include <utility>

#include <QSet>

#include "process/ProcessHelpers.h"

namespace gamelog::core::process
{
    void SteamProcessInspector::annotate(std::vector<ProcessInfo>& processes)
    {
        QSet<qint64> livePids;
        livePids.reserve(static_cast<qsizetype>(processes.size()));

        for (ProcessInfo& process: processes)
        {
            livePids.insert(process.pid);

            auto cached = cache_.find(process.pid);

            if (cached == cache_.end() || cached->executablePath != process.executablePath)
            {
                CacheEntry entry;
                entry.executablePath = process.executablePath;

                entry.steamAppId = ProcessHelpers::readSteamAppId(process.pid);

                cached = cache_.insert(process.pid, std::move(entry));
            }

            process.steamAppId = cached->steamAppId;
        }

        for (auto cached = cache_.begin(); cached != cache_.end();)
        {
            if (!livePids.contains(cached.key()))
            {
                cached = cache_.erase(cached);
            }
            else
            {
                ++cached;
            }
        }
    }
} // namespace gamelog::core::process
