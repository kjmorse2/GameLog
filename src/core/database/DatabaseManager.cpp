#include "database/DatabaseManager.h"

#include "database/DatabaseMigrator.h"
#include "logging/LoggingCategories.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>

namespace gamelog::core::database
{

DatabaseManager::DatabaseManager(
    QString databasePath,
    QString connectionName
)
    : databasePath_{std::move(databasePath)},
      connectionName_{std::move(connectionName)}
{
}

DatabaseManager::~DatabaseManager()
{
    const QString connectionName = connectionName_;

    if (database_.isValid())
    {
        database_.close();
    }

    // All QSqlDatabase handles must release the connection before
    // removeDatabase() is called.
    database_ = QSqlDatabase{};

    if (!connectionName.isEmpty() &&
        QSqlDatabase::contains(connectionName))
    {
        QSqlDatabase::removeDatabase(connectionName);
    }
}

bool DatabaseManager::initialize()
{
    if (!openDatabase())
    {
        return false;
    }

    if (!configureDatabase())
    {
        return false;
    }

    return runMigrations();
}

bool DatabaseManager::isOpen() const
{
    return database_.isOpen();
}

QSqlDatabase DatabaseManager::database() const
{
    return database_;
}

bool DatabaseManager::openDatabase()
{
    if (connectionName_.isEmpty())
    {
        qCWarning(gamelogDatabaseLog)
            << "Cannot open database with an empty connection name.";
        return false;
    }

    if (QSqlDatabase::contains(connectionName_))
    {
        qCWarning(gamelogDatabaseLog)
            << "A database connection already exists with name:"
            << connectionName_;
        return false;
    }

    database_ = QSqlDatabase::addDatabase(
        "QSQLITE",
        connectionName_
    );
    database_.setDatabaseName(databasePath_);

    if (!database_.open())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to open database:"
            << database_.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseManager::configureDatabase()
{
    QSqlQuery query{database_};

    if (!query.exec("PRAGMA foreign_keys = ON"))
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to enable foreign keys:"
            << query.lastError().text();

        return false;
    }

    if (!query.exec("PRAGMA busy_timeout = 5000"))
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to set SQLite busy timeout:"
            << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseManager::runMigrations()
{
    DatabaseMigrator migrator{database_};
    return migrator.applyPendingMigrations();
}

} // namespace gamelog::core::database
