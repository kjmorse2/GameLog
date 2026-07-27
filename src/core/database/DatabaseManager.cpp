#include "database/DatabaseManager.h"

#include <QLoggingCategory>
#include <utility>

#include "logging/LoggingCategories.h"

namespace gamelog::core::database
{
DatabaseManager::DatabaseManager(QString databasePath)
    : m_databasePath(std::move(databasePath))
{
}

bool DatabaseManager::initialize()
{
    qCWarning(gamelogDatabaseLog) << "Database initialization is not implemented yet:" << m_databasePath;
    // TODO: Create/open SQLite database and apply migrations.
    return true;
}

const QString &DatabaseManager::databasePath() const
{
    return m_databasePath;
}
} // namespace gamelog::core::database
