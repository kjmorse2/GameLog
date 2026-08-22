#pragma once

#include "domain/Session.h"
#include "domain/query/QueryOptions.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

#include <QDateTime>
#include <QDebug>

namespace gamelog::core::domain::query
{
    /**
     * @brief The field to sort sessions by.
     */
    enum class SessionSortField
    {
        StartTimestamp, TrackedDuration, Id
    };

    /**
     * Allows for easy logging of SessionSortField objects using QDebug.
     * @param debug The QDebug stream to write to.
     * @param sortField The SessionSortField object to log.
     * @return The QDebug stream after writing the SessionSortField object.
     */
    QDebug operator<<(QDebug debug, SessionSortField sortField);

    /**
     * Persistence-neutral description of a session search.
     *
     * Set fields are combined with AND. Values within gameIds, sources, or
     * statuses are combined with IN.
     */
    struct SessionQuery
    {
        /**
         * The IDs of the sessions to select.
         */
        std::vector<int> ids{};

        /**
         * The IDs of the games to select.
         */
        std::vector<int> gameIds{};

        /**
         * The sources of the sessions to select.
         */
        std::vector<SessionSource> sources{};

        /**
         * The statuses of the sessions to select.
         */
        std::vector<SessionStatus> statuses{};

        /**
         * The inclusive lower bound for session start timestamps.
         */
        std::optional<QDateTime> startedAtOrAfter{};

        /**
         * The exclusive upper bound for session start timestamps.
         */
        std::optional<QDateTime> startedBefore{};

        /**
         * The minimum tracked duration of the sessions to select
         */
        std::optional<std::chrono::seconds> minimumTrackedDuration{};

        /**
         * The maximum tracked duration of the sessions to select
         */
        std::optional<std::chrono::seconds> maximumTrackedDuration{};

        /**
         * Whether the sessions to select have an end timestamp
         */
        std::optional<bool> hasEndTimestamp{};

        /**
         * The field to sort by.
         */
        SessionSortField sortBy{SessionSortField::StartTimestamp};

        /**
         * The direction to sort by.
         */
        SortDirection sortDirection{SortDirection::Descending};

        /**
         * The maximum number of sessions to return.
         */
        std::optional<std::size_t> limit{};

        /**
         * The number of sessions to skip before returning the first one.
         */
        std::optional<std::size_t> offset{};
    };

    /**
     * Allows for easy logging of SessionQuery objects using QDebug.
     * @param debug The QDebug stream to write to.
     * @param query The SessionQuery object to log.
     * @return The QDebug stream after writing the SessionQuery object.
     */
    QDebug operator<<(QDebug debug, const SessionQuery& query);
} // namespace gamelog::core::domain::query
