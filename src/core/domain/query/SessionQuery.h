#pragma once

#include "domain/Session.h"
#include "domain/query/QueryOptions.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

#include <QDateTime>

namespace gamelog::core::domain::query
{
    enum class SessionSortField
    {
        StartTimestamp,
        TrackedDuration,
        Id
    };

    /**
 * Persistence-neutral description of a session search.
 *
 * Set fields are combined with AND. Values within gameIds, sources, or
 * statuses are combined with IN.
 */
    struct SessionQuery
    {
        /**
     * The Id's of sessions to select
     */
        std::vector<int> ids;

        /**
     * The Id's of Games to select
     */
        std::vector<int> gameIds;

        /**
     * The sources of the sessions to select
     */
        std::vector<SessionSource> sources;

        /**
     * The status's of games to select
     */
        std::vector<SessionStatus> statuses;

        /**
     * The earliest start time of the sessions to select
     */
        std::optional<QDateTime> startedAtOrAfter;

        /**
     * The latest start time of the sessions to select
     */
        std::optional<QDateTime> startedBefore;

        /**
     * The minimum tracked duration of the sessions to select
     */
        std::optional<std::chrono::seconds> minimumTrackedDuration;

        /**
     * The maximum tracked duration of the sessions to select
     */
        std::optional<std::chrono::seconds> maximumTrackedDuration;

        /**
     * Whether the sessions to select have an end timestamp
     */
        std::optional<bool> hasEndTimestamp;

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
        std::optional<std::size_t> limit;

        /**
     * The number of sessions to skip before returning the first one.
     */
        std::optional<std::size_t> offset;
    };
} // namespace gamelog::core::domain::query
