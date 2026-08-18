#pragma once

#include <QDebug>

namespace gamelog::core::domain::query
{
    /**
 * @brief The direction to sort by.
 */
    enum class SortDirection
    {
        Ascending, Descending
    };

    /**
     * Allows for easy logging of SortDirection objects using QDebug.
     * @param debug The QDebug stream to write to.
     * @param direction The SortDirection object to log.
     * @return The QDebug stream after writing the SortDirection object.
     */
    QDebug operator<<(QDebug debug, SortDirection direction);
} // namespace gamelog::core::domain::query
