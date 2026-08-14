#pragma once

#include "domain/query/QueryOptions.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <QDebug>
#include <QString>

namespace gamelog::core::domain::query
{
    enum class GameSortField
    {
        Title,
        Id
    };

   QDebug operator<<(QDebug debug, const GameSortField sortField);

   /**
 * Persistence-neutral description of a game search.
 *
 * Set fields are combined with AND. Multiple IDs are combined with IN.
 * SQL details deliberately do not appear in this type.
 */
    struct GameQuery
    {
        /**
     * @brief The ID's to select
     */
        std::vector<std::int64_t> ids;

        /**
     * @brief the title of the Game to select
     */
        std::optional<QString> title;

        /**
     * @brief The name of the executable to select
     */
        std::optional<QString> executableName;

        /**
     * @brief The path of the executable to select
     */
        std::optional<QString> executablePath;

        /**
     * @brief The Steam App ID of the game to select
     */
        std::optional<int> steamAppId;

        /**
     * @brief Whether the game is being tracked
     */
        std::optional<bool> trackingEnabled;

        /**
     * @brief The field to sort by
     */
        GameSortField sortBy{GameSortField::Title};

        /**
        * @brief The direction to sort by
        */
        SortDirection sortDirection{SortDirection::Ascending};

        /**
        * @brief limit of Games to select.
        */
        std::optional<std::size_t> limit;

        /**
     * @brief The number of Games to skip.
     */
        std::optional<std::size_t> offset;
    };

    QDebug operator<<(QDebug debug, const GameQuery &query);
} // namespace gamelog::core::domain::query
