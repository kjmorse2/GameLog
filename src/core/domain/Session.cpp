#include "Session.h"

#include <QDebugStateSaver>

#include <stdexcept>

namespace
{
    QString sessionSourceToString(const gamelog::core::domain::SessionSource source)
    {
        switch(source)
        {
            case gamelog::core::domain::SessionSource::Automatic :
                return QStringLiteral("Automatic");
            case gamelog::core::domain::SessionSource::Manual :
                return QStringLiteral("Manual");
        }

        return QStringLiteral("Unknown");
    }

    QString sessionStatusToString(const gamelog::core::domain::SessionStatus status)
    {
        switch(status)
        {
            case gamelog::core::domain::SessionStatus::Active :
                return QStringLiteral("Active");
            case gamelog::core::domain::SessionStatus::Completed :
                return QStringLiteral("Completed");
            case gamelog::core::domain::SessionStatus::Interrupted :
                return QStringLiteral("Interrupted");
        }

        return QStringLiteral("Unknown");
    }
}

namespace gamelog::core::domain
{
    QDebug operator<<(QDebug debug, const SessionSource source)
    {
        QDebugStateSaver saver{debug};
        debug.nospace() << sessionSourceToString(source);
        return debug;
    }

    QDebug operator<<(QDebug debug, const SessionStatus status)
    {
        QDebugStateSaver saver{debug};
        debug.nospace() << sessionStatusToString(status);
        return debug;
    }

    SessionSource sessionSourceFromString(const QString& sourceString)
    {
        if(sourceString == "automatic" || sourceString == "Automatic")
        {
            return SessionSource::Automatic;
        }
        if(sourceString == "manual" || sourceString == "Manual")
        {
            return SessionSource::Manual;
        }
        throw std::invalid_argument("Invalid session source string: " + sourceString.toStdString());
    }

    SessionStatus sessionStatusFromString(const QString& statusString)
    {
        if(statusString == "active" || statusString == "Active")
        {
            return SessionStatus::Active;
        }
        if(statusString == "completed" || statusString == "Completed")
        {
            return SessionStatus::Completed;
        }
        if(statusString == "interrupted" || statusString == "Interrupted")
        {
            return SessionStatus::Interrupted;
        }
        throw std::invalid_argument("Invalid session status string: " + statusString.toStdString());
    }

    QDebug operator<<(QDebug debug, const Session& session)
    {
        QDebugStateSaver saver{debug};

        debug.nospace() << "Session {"
                << "id: " << session.id
                << ", gameId: " << session.gameId
                << ", startTimestamp: " << session.startTimestamp
                << ", endTimestamp: " << (session.endTimestamp.has_value() ? session.endTimestamp->toString(Qt::ISODateWithMs) : QStringLiteral("nullopt"))
                << ", trackedDuration: " << session.trackedDuration.count()
                << ", source: " << session.source
                << ", status: " << session.status
                << ", notes: " << session.notes
                << "}";
        return debug;
    }
} // namespace gamelog::core::domain
