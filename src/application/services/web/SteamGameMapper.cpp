#include "application/services/web/SteamGameMapper.h"

#include <QJsonObject>
#include <QJsonValue>

using gamelog::core::domain::Game;

namespace gamelog::application::services
{
    std::vector<Game> gamesFromSteamOwnedGames(const QJsonArray& steamGames)
    {
        std::vector<Game> games;
        games.reserve(static_cast<std::size_t>(steamGames.size()));

        for(const QJsonValue& value : steamGames)
        {
            if(!value.isObject()) { continue; }

            const QJsonObject object = value.toObject();
            const int appId = object.value(QStringLiteral("appid")).toInt();
            const QString title = object.value(QStringLiteral("name")).toString();

            if(appId <= 0 || title.trimmed().isEmpty()) { continue; }

            Game game;
            game.title = title;
            game.steamAppId = appId;
            games.push_back(std::move(game));
        }

        return games;
    }
} // namespace gamelog::application::services
