#pragma once

#include <chrono>
#include <optional>

#include <QDateTime>
#include <QDebug>

namespace gamelog::core::domain
{
    enum class SessionSource
    {
        Automatic, Manual
    };

    enum class SessionStatus
    {
        Active, Completed, Interrupted
    };

    QDebug operator<<(QDebug debug, SessionSource source);

    QDebug operator<<(QDebug debug, SessionStatus status);

    /**
     * @brief Converts a string to a SessionSource enum.
     * @param sourceString the string to convert.
     * @return the corresponding SessionSource enum.
     */
    SessionSource sessionSourceFromString(const QString& sourceString);

    /**
     * @brief Converts a string to a SessionStatus enum.
     * @param statusString the string to convert.
     * @return the corresponding SessionStatus enum.
     */
    SessionStatus sessionStatusFromString(const QString& statusString);

    /**
     * @brief Represents one tracked play session.
     */
    struct Session
    {
        int id{0};
        int gameId{0};

        /**
         * @brief Session start time. Lifecycle code creates this value in UTC.
         */
        QDateTime startTimestamp;

        /**
         * @brief UTC end time; absent while the session is active.
         */
        std::optional<QDateTime> endTimestamp;

        /**
         * @brief Duration of the session that has been tracked so far. Lifecycle code updates this value while the session is active.
         */
        std::chrono::seconds trackedDuration{0};

        /**
         * @brief Source of the session, either automatic or manual. Lifecycle code sets this value when the session is created.
         */
        SessionSource source{SessionSource::Automatic};

        /**
         * @brief Status of the session, either active, completed, or interrupted. Lifecycle code updates this value when the session is completed or interrupted.
         */
        SessionStatus status{SessionStatus::Active};

        /**
         * @brief Optional notes about the session. Lifecycle code may set this value when the session is completed or interrupted.
         */
        QString notes{QStringLiteral("")};
    };

    /**
     * Allows for easy logging of Session objects using QDebug.
     * @param debug The QDebug stream to write to.
     * @param session The Session object to log.
     * @return The QDebug stream after writing the Session object.
     */
    QDebug operator<<(QDebug debug, const Session& session);
} // namespace gamelog::core::domain
