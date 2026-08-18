#include "database/DatabaseManager.h"

#include "database/DatabaseMigrator.h"
#include "logging/LoggingCategories.h"

#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <resources/AppPaths.h>

namespace gamelog::core::database
{
    DatabaseManager::DatabaseManager(QString databasePath, QString connectionName) : databasePath_{
            std::move(databasePath)
        },
        connectionName_{std::move(connectionName)} {}

    DatabaseManager::~DatabaseManager()
    {
        const QString connectionName = connectionName_;

        if(database_.isValid()) { database_.close(); }
        // All QSqlDatabase handles must release the connection before
        // removeDatabase() is called.
        database_ = QSqlDatabase{};
        if(!connectionName.isEmpty() && QSqlDatabase::contains(connectionName))
        {
            QSqlDatabase::removeDatabase(connectionName);
        }
    }

    bool DatabaseManager::initialize()
    {
        // Open the connection first so every later step has a live handle.
        if(!openDatabase()) { return false; }
        // Apply SQLite connection settings before any schema access happens.
        if(!configureDatabase()) { return false; }
        // Finally, bring the schema up to date.
        return runMigrations();
    }

    bool DatabaseManager::isOpen() const { return database_.isOpen(); }

    QSqlDatabase DatabaseManager::database() const { return database_; }

    QString DatabaseManager::defaultDatabasePath()
    {
        // AppLocalDataLocation is the portable default for a per-user SQLite file.
        const QString dataDirectory = AppPaths::dataDirectory();

        if(dataDirectory.isEmpty()) { return {}; }

        // Ensure the directory exists before returning the final database file path.

        if(const QDir directory; !directory.mkpath(dataDirectory)) { return {}; }

        return AppPaths::databasePath();
    }

    QString DatabaseManager::resolveDatabasePath(const QString& commandLinePath)
    {
        // Command-line overrides win when they are present.
        if(!commandLinePath.isEmpty()) { return QFileInfo{commandLinePath}.absoluteFilePath(); }

        // Allow local development and test runs to redirect storage.

        if(const QString environmentPath = qEnvironmentVariable("GAMELOG_DATABASE_PATH"); !environmentPath.isEmpty())
        {
            return QFileInfo{environmentPath}.absoluteFilePath();
        }

        // Fall back to the default per-user location.
        return defaultDatabasePath();
    }

    bool DatabaseManager::openDatabase()
    {
        if(connectionName_.isEmpty())
        {
            qCWarning(gamelogDatabaseLog) << "Cannot open database with an empty connection name.";
            return false;
        }

        if(QSqlDatabase::contains(connectionName_))
        {
            qCWarning(gamelogDatabaseLog) << "A database connection already exists with name:" << connectionName_;
            return false;
        }

        // Use a named Qt connection so the manager can clean it up deterministically.
        database_ = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
        database_.setDatabaseName(databasePath_);

        if(!database_.open())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to open database:" << database_.lastError().text();
            return false;
        }
        return true;
    }

    bool DatabaseManager::configureDatabase()
    {
        // Foreign keys are required for the repository relationships.
        QSqlQuery query{database_};

        if(!query.exec("PRAGMA foreign_keys = ON"))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to enable foreign keys:" << query.lastError().text();
            return false;
        }
        // Keep SQLite from failing immediately when a second writer appears.
        if(!query.exec("PRAGMA busy_timeout = 5000"))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to set SQLite busy timeout:" << query.lastError().text();
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
