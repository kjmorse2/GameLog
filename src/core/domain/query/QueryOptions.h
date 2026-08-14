#pragma once

#include <QDebug>

namespace gamelog::core::domain::query
{
    /**
 * @brief The direction to sort by.
 */
    enum class SortDirection
    {
        Ascending,
        Descending
    };

    QDebug operator<<(QDebug debug, SortDirection direction);
} // namespace gamelog::core::domain::query
