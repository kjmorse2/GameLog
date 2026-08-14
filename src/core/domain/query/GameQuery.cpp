#include "GameQuery.h"

#include <QDebugStateSaver>

namespace {
    QString optionalString(const std::optional<QString>& value)
    {
        return value.has_value() ? *value : QStringLiteral("nullopt");
    }

    QString optionalBoolString(const std::optional<bool>& value)
    {
        return value.has_value() ? (value.value() ? QStringLiteral("true") : QStringLiteral("false")) : QStringLiteral("nullopt");
    }

    QString optionalSizeString(const std::optional<std::size_t>& value)
    {
        return value.has_value() ? QString::number(*value) : QStringLiteral("nullopt");
    }

    template<typename T>
    QString vectorString(const std::vector<T>& values)
    {
        QString output = "[";
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index > 0)
            {
                output += ", ";
            }
            output += QString::number(static_cast<long long>(values[index]));
        }
        output += "]";
        return output;
    }
}

namespace gamelog::core::domain::query
{
    QDebug operator<<(QDebug debug, const GameSortField sortField)
    {
        QDebugStateSaver saver {debug};

        switch (sortField)
        {
            case GameSortField::Title:
                debug.nospace() << "Title";
                break;
            case GameSortField::Id:
                debug.nospace() << "Id";
                break;
        }

        return debug;
    }

    QDebug operator<<(QDebug debug, const GameQuery &query)
    {
        QDebugStateSaver saver {debug};

        debug.nospace() << "GameQuery {"
                        << "ids: " << vectorString(query.ids)
                        << ", title: " << optionalString(query.title)
                        << ", executableName: " << optionalString(query.executableName)
                        << ", executablePath: " << optionalString(query.executablePath)
                        << ", steamAppId: " << (query.steamAppId.has_value() ? QString::number(*query.steamAppId) : QStringLiteral("nullopt"))
                        << ", trackingEnabled: " << optionalBoolString(query.trackingEnabled)
                        << ", sortBy: " << query.sortBy
                        << ", sortDirection: " << query.sortDirection
                        << ", limit: " << optionalSizeString(query.limit)
                        << ", offset: " << optionalSizeString(query.offset)
                        << "}";
        return debug;
    }
} // namespace gamelog::core::domain::query
