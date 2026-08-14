#include "process/ProcessHelpers.h"

#include <limits>

#include <QFile>
#include <QIODevice>
#include <QList>
#include <QString>

#include "process/ProcessInfo.h"

namespace gamelog::core::process
{
    std::optional<QByteArray> ProcessHelpers::readProcessEnvironmentValue(qint64 pid, const QByteArray& variableName)
    {
        if(pid <= 0 || variableName.isEmpty())
        {
            return std::nullopt;
        }

        QFile environmentFile{QStringLiteral("/proc/%1/environ").arg(pid)};
        if(!environmentFile.open(QIODevice::ReadOnly))
        {
            return std::nullopt;
        }

        const QByteArray environment = environmentFile.readAll();
        const QByteArray prefix = variableName + '=';
        const QList<QByteArray> entries = environment.split('\0');

        for(const QByteArray& entry : entries)
        {
            if(entry.startsWith(prefix))
            {
                return entry.mid(prefix.size());
            }
        }

        return std::nullopt;
    }

    std::optional<std::uint32_t> ProcessHelpers::readSteamAppId(qint64 pid)
    {
        const auto value = readProcessEnvironmentValue(pid, QByteArrayLiteral("SteamAppId"));
        if(!value)
        {
            return std::nullopt;
        }

        bool parsed = false;
        const qulonglong numericValue = value->toULongLong(&parsed);

        if(!parsed || numericValue == 0 || numericValue > std::numeric_limits<std::uint32_t>::max())
        {
            return std::nullopt;
        }

        return static_cast<std::uint32_t>(numericValue);
    }

    const domain::Game* ProcessHelpers::matchTrackedGame(
            const ProcessInfo& process,
            const QHash<std::uint32_t, domain::Game>& trackedSteamGames,
            const QHash<QString, domain::Game>& trackedPathGames
            ) noexcept
    {
        if(process.steamAppId)
        {
            const auto steamGame = trackedSteamGames.constFind(*process.steamAppId);
            if(steamGame != trackedSteamGames.constEnd())
            {
                return &steamGame.value();
            }
        }

        if(!process.executablePath.isEmpty())
        {
            const auto pathGame = trackedPathGames.constFind(process.executablePath);
            if(pathGame != trackedPathGames.constEnd())
            {
                return &pathGame.value();
            }
        }

        return nullptr;
    }

    bool ProcessHelpers::processMatchesGame(const ProcessInfo& process, const domain::Game& game) noexcept
    {
        if(game.steamAppId && *game.steamAppId > 0 && process.steamAppId)
        {
            return *process.steamAppId == static_cast<std::uint32_t>(*game.steamAppId);
        }

        return !game.executablePath.isEmpty() && process.executablePath == game.executablePath;
    }
} // namespace gamelog::core::process
