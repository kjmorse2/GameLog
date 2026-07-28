#include "database/DatabaseManager.h"

#include <QLoggingCategory>
#include <utility>

#include "logging/LoggingCategories.h"
#include <QtCore/qloggingcategory.h>

namespace gamelog::core::database
{
DatabaseManager::DatabaseManager(QString databasePath)
    : databasePath_(std::move(databasePath))
{
}

bool DatabaseManager::openDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    db.setDatabaseName(databasePath_);

    if (!db.open())
    {
        qCWarning(gamelogDatabaseLog) << "Failed to open database:" << db.lastError().text();
        return false;
    }

    database_ = db;

    return true;
}

bool DatabaseManager::initialize()
{
    if (!openDatabase())
    {
        return false;
    }

    if (!runMigrations())
    {
        return false;
    }

    return true;
}

const QString& DatabaseManager::databasePath() const
{
    return databasePath_;
}

bool DatabaseManager::isOpen() const
{
    return database().isOpen();
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(connectionName_);
}

bool DatabaseManager::runMigrations()
{
    return true;
} // namespace gamelog::core::database

}