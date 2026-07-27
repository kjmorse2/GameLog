#pragma once

#include <optional>

#include <QString>

namespace gamelog::core::domain
{
struct Game
{
    int id{0};
    QString title;
    QString executablePath;
    QString executableName;
    std::optional<int> steamAppId;
    std::optional<QString> artworkPath;
    bool trackingEnabled{true};
};
} // namespace gamelog::core::domain
