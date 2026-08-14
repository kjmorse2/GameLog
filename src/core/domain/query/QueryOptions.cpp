#include "QueryOptions.h"

#include <QDebugStateSaver>

namespace
{
    QString sortDirectionToString(const gamelog::core::domain::query::SortDirection direction)
    {
        switch(direction)
        {
            case gamelog::core::domain::query::SortDirection::Ascending :
                return QStringLiteral("Ascending");
            case gamelog::core::domain::query::SortDirection::Descending :
                return QStringLiteral("Descending");
        }

        return QStringLiteral("Unknown");
    }
}

namespace gamelog::core::domain::query
{
    QDebug operator<<(QDebug debug, const SortDirection direction)
    {
        QDebugStateSaver saver{debug};
        debug.nospace() << sortDirectionToString(direction);
        return debug;
    }
} // namespace gamelog::core::domain::query
