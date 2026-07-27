#pragma once

#include <chrono>
#include <optional>

#include <QDateTime>

namespace gamelog::core::domain
{
enum class SessionSource
{
    Automatic,
    Manual
};

enum class SessionStatus
{
    Active,
    Completed,
    Interrupted
};

struct Session
{
    int id{0};
    int gameId{0};
    QDateTime startTimestamp;
    std::optional<QDateTime> endTimestamp;
    std::chrono::seconds trackedDuration{0};
    SessionSource source{SessionSource::Automatic};
    SessionStatus status{SessionStatus::Active};
};
} // namespace gamelog::core::domain
