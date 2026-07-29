#pragma once

#include <chrono>
#include <optional>

#include <QDateTime>
#include <QString>

namespace gamelog::core::domain {

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

    SessionSource sessionSourceFromString(const QString &sourceString);
    SessionStatus sessionStatusFromString(const QString &statusString);

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

        std::chrono::seconds trackedDuration{0};
        SessionSource source{SessionSource::Automatic};
        SessionStatus status{SessionStatus::Active};
    };

} // namespace gamelog::core::domain
