#pragma once

#include <QDateTime>
#include <QDebug>
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

    /**
     * Allows for easy logging of SessionDocument objects using QDebug.
     * @param debug The QDebug stream to write to.
     * @param document The SessionDocument object to log.
     * @return The QDebug stream after writing the SessionDocument object.
     */
    QDebug operator<<(QDebug debug, const SessionDocument& document);
} // namespace gamelog::core::domain
