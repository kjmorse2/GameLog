#pragma once

#include <chrono>
#include <optional>

#include <QDateTime>
#include <QString>

namespace gamelog::core::domain {
    /**
     * @brief Identifies how a session was created.
     */
    enum class SessionSource
    {
        Automatic,
        Manual
    };

    /**
     * @brief Tracks the lifecycle state of a session.
     */
    enum class SessionStatus
    {
        Active,
        Completed,
        Interrupted
    };

    /**
     * @brief Converts a stored session-source string into an enum value.
     * @param sourceString Serialized session source, usually from the database.
     * @return The matching SessionSource value.
     * @throws std::invalid_argument when the string is not recognized.
     */
    SessionSource sessionSourceFromString(const QString &sourceString);

    /**
     * @brief Converts a stored session-status string into an enum value.
     * @param statusString Serialized session status, usually from the database.
     * @return The matching SessionStatus value.
     * @throws std::invalid_argument when the string is not recognized.
     */
    SessionStatus sessionStatusFromString(const QString &statusString);

    /**
     * @brief Represents one tracked play session.
     */
    struct Session
    {
        /**
         * @brief Primary key from the sessions table.
         */
        int id{0};

        /**
         * @brief Primary key of the associated game.
         */
        int gameId{0};

        /**
         * @brief Start timestamp stored in local application time.
         */
        QDateTime startTimestamp;

        /**
         * @brief Optional end timestamp for completed sessions.
         */
        std::optional<QDateTime> endTimestamp;

        /**
         * @brief Duration accumulated while the session was active.
         */
        std::chrono::seconds trackedDuration{0};

        /**
         * @brief Source used to start the session.
         */
        SessionSource source{SessionSource::Automatic};

        /**
         * @brief Current lifecycle state.
         */
        SessionStatus status{SessionStatus::Active};
    };
} // namespace gamelog::core::domain
