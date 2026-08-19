#include "SessionQuery.h"

#include <QDebugStateSaver>

namespace
{
    QString optionalBoolString(const std::optional<bool>& value)
    {
        return value.has_value()
                   ? (value.value() ? QStringLiteral("true") : QStringLiteral("false"))
                   : QStringLiteral("nullopt");
    }

    QString optionalDurationString(const std::optional<std::chrono::seconds>& value)
    {
        return value.has_value() ? QString::number(value->count()) : QStringLiteral("nullopt");
    }

    QString optionalSizeString(const std::optional<std::size_t>& value)
    {
        return value.has_value() ? QString::number(*value) : QStringLiteral("nullopt");
    }

    QString toStringValue(const gamelog::core::domain::SessionSource source)
    {
        return gamelog::core::domain::toDisplayString(source);
    }

    QString toStringValue(const gamelog::core::domain::SessionStatus status)
    {
        return gamelog::core::domain::toDisplayString(status);
    }

    QString toStringValue(const int value) { return QString::number(value); }

    template <typename T> QString vectorString(const std::vector<T>& values)
    {
        QString output = "[";
        for(std::size_t index = 0; index < values.size(); ++index)
        {
            if(index > 0) { output += ", "; }
            output += toStringValue(values[index]);
        }
        output += "]";
        return output;
    }
}

namespace gamelog::core::domain::query
{
    QDebug operator<<(QDebug debug, const SessionSortField sortField)
    {
        QDebugStateSaver saver{debug};

        switch(sortField)
        {
        case SessionSortField::StartTimestamp:
            debug.nospace() << "StartTimestamp";
            break;
        case SessionSortField::TrackedDuration:
            debug.nospace() << "TrackedDuration";
            break;
        case SessionSortField::Id:
            debug.nospace() << "Id";
            break;
        }

        return debug;
    }

    QDebug operator<<(QDebug debug, const SessionQuery& query)
    {
        QDebugStateSaver saver{debug};

        debug.nospace() << "SessionQuery {" << "ids: " << vectorString(query.ids) << ", gameIds: " <<
            vectorString(query.gameIds) << ", sources: " << vectorString(query.sources) << ", statuses: " <<
            vectorString(query.statuses) << ", startedAtOrAfter: " << (
                query.startedAtOrAfter.has_value()
                    ? query.startedAtOrAfter->toString(Qt::ISODateWithMs)
                    : QStringLiteral("nullopt")) << ", startedBefore: " << (
                query.startedBefore.has_value()
                    ? query.startedBefore->toString(Qt::ISODateWithMs)
                    : QStringLiteral("nullopt")) << ", minimumTrackedDuration: " <<
            optionalDurationString(query.minimumTrackedDuration) << ", maximumTrackedDuration: " <<
            optionalDurationString(query.maximumTrackedDuration) << ", hasEndTimestamp: " <<
            optionalBoolString(query.hasEndTimestamp) << ", sortBy: " << query.sortBy << ", sortDirection: " << query.
            sortDirection << ", limit: " << optionalSizeString(query.limit) << ", offset: " <<
            optionalSizeString(query.offset) << "}";
        return debug;
    }
} // namespace gamelog::core::domain::query
