#pragma once

#include "domain/query/QueryOptions.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <QString>

namespace gamelog::core::domain::query {

enum class GameSortField
{
    Title,
    Id
};

/**
 * Persistence-neutral description of a game search.
 *
 * Set fields are combined with AND. Multiple IDs are combined with IN.
 * SQL details deliberately do not appear in this type.
 */
struct GameQuery
{
    std::vector<std::int64_t> ids;
    std::optional<QString> title;
    std::optional<QString> executableName;
    std::optional<QString> executablePath;
    std::optional<int> steamAppId;
    std::optional<bool> trackingEnabled;

    GameSortField sortBy{GameSortField::Title};
    SortDirection sortDirection{SortDirection::Ascending};
    std::optional<std::size_t> limit;
    std::optional<std::size_t> offset;
};

} // namespace gamelog::core::domain::query
