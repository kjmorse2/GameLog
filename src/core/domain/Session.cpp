#include "Session.h"

#include <QDebugStateSaver>

#include <stdexcept>

namespace gamelog::core::domain
{
    QString toDisplayString(const SessionSource source)
    {
        switch(source)
        {
        case SessionSource::Automatic:
            return QStringLiteral("Automatic");
        case SessionSource::Manual:
            return QStringLiteral("Manual");
        }

        return QStringLiteral("Unknown");
    }

    QString toDisplayString(const SessionStatus status)
    {
        switch(status)
        {
        case SessionStatus::Active:
            return QStringLiteral("Active");
        case SessionStatus::Completed:
            return QStringLiteral("Completed");
        case SessionStatus::Interrupted:
            return QStringLiteral("Interrupted");
        }

        return QStringLiteral("Unknown");
    }

    QString toDatabaseString(const SessionSource source)
    {
        switch(source)
        {
        case SessionSource::Automatic:
            return QStringLiteral("automatic");
        case SessionSource::Manual:
            return QStringLiteral("manual");
        }

        return QStringLiteral("automatic");
    }

    QString toDatabaseString(const SessionStatus status)
    {
        switch(status)
        {
        case SessionStatus::Active:
            return QStringLiteral("active");
        case SessionStatus::Completed:
            return QStringLiteral("completed");
        case SessionStatus::Interrupted:
            return QStringLiteral("interrupted");
        }

        return QStringLiteral("interrupted");
    }

    std::optional<SessionSource> sessionSourceFromDatabase(const QString& value)
    {
        if(value == QStringLiteral("automatic")) { return SessionSource::Automatic; }
        if(value == QStringLiteral("manual")) { return SessionSource::Manual; }
        return std::nullopt;
    }

    std::optional<SessionStatus> sessionStatusFromDatabase(const QString& value)
    {
        if(value == QStringLiteral("active")) { return SessionStatus::Active; }
        if(value == QStringLiteral("completed")) { return SessionStatus::Completed; }
        if(value == QStringLiteral("interrupted")) { return SessionStatus::Interrupted; }
        return std::nullopt;
    }

    QDebug operator<<(QDebug debug, const SessionSource source)
    {
        QDebugStateSaver saver{debug};
        debug.nospace() << toDisplayString(source);
        return debug;
    }

    QDebug operator<<(QDebug debug, const SessionStatus status)
    {
        QDebugStateSaver saver{debug};
        debug.nospace() << toDisplayString(status);
        return debug;
    }

    SessionSource sessionSourceFromString(const QString& sourceString)
    {
        if(sourceString == "automatic" || sourceString == "Automatic") { return SessionSource::Automatic; }
        if(sourceString == "manual" || sourceString == "Manual") { return SessionSource::Manual; }
        throw std::invalid_argument("Invalid session source string: " + sourceString.toStdString());
    }

    SessionStatus sessionStatusFromString(const QString& statusString)
    {
        if(statusString == "active" || statusString == "Active") { return SessionStatus::Active; }
        if(statusString == "completed" || statusString == "Completed") { return SessionStatus::Completed; }
        if(statusString == "interrupted" || statusString == "Interrupted") { return SessionStatus::Interrupted; }
        throw std::invalid_argument("Invalid session status string: " + statusString.toStdString());
    }

    QDebug operator<<(QDebug debug, const Session& session)
    {
        QDebugStateSaver saver{debug};

        debug.nospace() << "Session {" << "id: " << session.id << ", gameId: " << session.gameId << ", startTimestamp: "
            << session.startTimestamp << ", endTimestamp: " << (session.endTimestamp.has_value()
                                                                    ? session.endTimestamp->toString(Qt::ISODateWithMs)
                                                                    : QStringLiteral("nullopt")) <<
            ", trackedDuration: " << session.trackedDuration.count() << ", source: " << session.source << ", status: "
            << session.status << ", notes: " << session.notes << "}";
        return debug;
    }
} // namespace gamelog::core::domain
