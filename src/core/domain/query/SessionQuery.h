#pragma once

#include "domain/Session.h"
#include "domain/query/QueryOptions.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

#include <QDateTime>

namespace gamelog::core::domain::query {

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
    std::vector<int> ids;
    std::vector<int> gameIds;
    std::vector<SessionSource> sources;
    std::vector<SessionStatus> statuses;

    std::optional<QDateTime> startedAtOrAfter;
    std::optional<QDateTime> startedBefore;
    std::optional<std::chrono::seconds> minimumTrackedDuration;
    std::optional<std::chrono::seconds> maximumTrackedDuration;
    std::optional<bool> hasEndTimestamp;

    SessionSortField sortBy{SessionSortField::StartTimestamp};
    SortDirection sortDirection{SortDirection::Descending};
    std::optional<std::size_t> limit;
    std::optional<std::size_t> offset;
};

} // namespace gamelog::core::domain::query
