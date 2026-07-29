#pragma once

#include <QDateTime>
#include <QString>

namespace gamelog::core::domain
{
/**
 * @brief Stores the rich-text note document associated with a session.
 */
struct SessionDocument
{
    /**
     * @brief Primary key of the owning session.
     */
    int sessionId{0};

    /**
     * @brief HTML payload written to disk or the database.
     */
    QString htmlContent;

    /**
     * @brief UTC timestamp of the last save operation.
     */
    QDateTime lastSavedTimestamp;
};
} // namespace gamelog::core::domain
