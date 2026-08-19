#pragma once

#include <chrono>
#include <optional>

#include <QDateTime>
#include <QDebug>
#include <QMetaType>

namespace gamelog::core::domain
{
    enum class SessionSource
    {
        Automatic, Manual
    };

    enum class SessionStatus
    {
        Active, Completed, Interrupted
    };

    QDebug operator<<(QDebug debug, SessionSource source);
    QDebug operator<<(QDebug debug, SessionStatus status);

    /**
     * @brief Human-readable spelling of a source, for logging and display.
     *
     * Capitalized ("Automatic"). Never persisted; use toDatabaseString() for
     * anything that reaches the database.
     */
    [[nodiscard]] QString toDisplayString(SessionSource source);

    /**
     * @brief Human-readable spelling of a status, for logging and display.
     *
     * Capitalized ("Active"). Never persisted; use toDatabaseString() for
     * anything that reaches the database.
     */
    [[nodiscard]] QString toDisplayString(SessionStatus status);

    /**
     * @brief On-disk spelling of a source, written to sessions.source.
     *
     * Lowercase ("automatic"). These exact values are constrained by the
     * schema's CHECK on sessions.source, so changing one requires a migration.
     */
    [[nodiscard]] QString toDatabaseString(SessionSource source);

    /**
     * @brief On-disk spelling of a status, written to sessions.status.
     *
     * Lowercase ("active"). These exact values are constrained by the schema's
     * CHECK on sessions.status, so changing one requires a migration.
     */
    [[nodiscard]] QString toDatabaseString(SessionStatus status);

    /**
     * @brief Parses a persisted sessions.source value.
     * @return The source, or std::nullopt when the stored text is unrecognized
     * so callers can skip a corrupted row rather than fail outright.
     */
    [[nodiscard]] std::optional<SessionSource> sessionSourceFromDatabase(const QString& value);

    /**
     * @brief Parses a persisted sessions.status value.
     * @return The status, or std::nullopt when the stored text is unrecognized
     * so callers can skip a corrupted row rather than fail outright.
     */
    [[nodiscard]] std::optional<SessionStatus> sessionStatusFromDatabase(const QString& value);

    /**
     * @brief Converts an exact supported string to a SessionSource enum.
     *
     * Only "automatic"/"Automatic" and "manual"/"Manual" are accepted.
     * Input is not trimmed or made generally case-insensitive; invalid values
     * throw std::invalid_argument.
     * @param sourceString the string to convert.
     * @return the corresponding SessionSource enum.
     */
    SessionSource sessionSourceFromString(const QString& sourceString);

    /**
     * @brief Converts an exact supported string to a SessionStatus enum.
     *
     * Only lowercase and leading-capital forms of active, completed, and
     * interrupted are accepted. Invalid values throw std::invalid_argument.
     * @param statusString the string to convert.
     * @return the corresponding SessionStatus enum.
     */
    SessionStatus sessionStatusFromString(const QString& statusString);

    /**
     * @brief Represents one tracked play session.
     *
     * Persistence requires a valid start timestamp and a nonnegative tracked
     * duration. Active sessions have no end timestamp. Completed and interrupted
     * sessions have a valid end timestamp that is not earlier than their start.
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
         * @brief UTC end time; absent while active and required once inactive.
         */
        std::optional<QDateTime> endTimestamp;

        /**
         * @brief Nonnegative tracked duration. Automatic completion replaces it
         * with the wall-clock difference between start and end.
         */
        std::chrono::seconds trackedDuration{0};

        /**
         * @brief Source of the session, either automatic or manual.
         */
        SessionSource source{SessionSource::Automatic};

        /**
         * @brief Status of the session: active, completed, or interrupted.
         */
        SessionStatus status{SessionStatus::Active};

        /**
         * @brief Notes persisted in the corresponding session_documents row.
         *
         * The content is **Markdown**, which is what TextEditor reads and writes.
         * Migration 002 renamed the column from html_content to content and 003
         * removed the format column, so the format is a convention rather than a
         * stored property. Never null: a null QString would bind as SQL NULL
         * against a NOT NULL column, so the repository normalizes it to empty.
         */
        QString notes{QStringLiteral("")};
    };

    /**
     * Allows for easy logging of Session objects using QDebug.
     * @param debug The QDebug stream to write to.
     * @param session The Session object to log.
     * @return The QDebug stream after writing the Session object.
     */
    QDebug operator<<(QDebug debug, const Session& session);
} // namespace gamelog::core::domain

Q_DECLARE_METATYPE(gamelog::core::domain::Session)
