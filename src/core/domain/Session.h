#pragma once

#include <chrono>
#include <optional>

#include <QDateTime>

namespace gamelog::core::domain
{
/**
 * @brief The SessionSource enum represents the source of a session, either automatic or manual.
 */
enum class SessionSource
{
    Automatic,
    Manual
};

/**
 * @brief The SessionStatus enum represents the status of a session, which can be active, completed, or interrupted.
 */
enum class SessionStatus
{
    Active,
    Completed,
    Interrupted
};

/**
 * @brief Converts a string representation of a session source to the corresponding SessionSource enum value.
 * @param sourceString The string representation of the session source.
 * @return The corresponding SessionSource enum value.
 */
SessionSource sessionSourceFromString(const QString &sourceString);

/**
 * @brief Converts a string representation of a session status to the corresponding SessionStatus enum value.
 * @param statusString The string representation of the session status.
 * @return The corresponding SessionStatus enum value.
 */
SessionStatus sessionStatusFromString(const QString &statusString);

/**
 * @brief The Session struct represents a gaming session in the application.
 */
struct Session
{
    /**
     * @brief The unique identifier for the session.
     */
    int id{0};

    /**
     * @brief The unique identifier for the game associated with the session.
     */
    int gameId{0};

    /**
     * @brief The timestamp when the session started.
     */
    QDateTime startTimestamp;

    /**
     * @brief The timestamp when the session ended, if available.
     */
    std::optional<QDateTime> endTimestamp;

    /**
     * @brief The total duration of the session that was tracked, in seconds.
     */
    std::chrono::seconds trackedDuration{0};

    /**
     * @brief The source of the session, either automatic or manual.
     */
    SessionSource source{SessionSource::Automatic};

    /**
     * @brief The status of the session, which can be active, completed, or interrupted.
     */
    SessionStatus status{SessionStatus::Active};
};
} // namespace gamelog::core::domain
