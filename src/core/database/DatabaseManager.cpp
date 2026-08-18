#include "database/DatabaseManager.h"

#include "database/DatabaseMigrator.h"
#include "logging/LoggingCategories.h"
#include "resources/AppPaths.h"

#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace gamelog::core::database
{
    DatabaseManager::DatabaseManager(QString databasePath, QString connectionName)
        : databasePath_{std::move(databasePath)},
          connectionName_{std::move(connectionName)} {}

    DatabaseManager::~DatabaseManager() { closeDatabase(); }

    bool DatabaseManager::initialize()
    {
        if(initialized_&& database_.isOpen())
        {
            return true;
        }

        if(databasePath_.trimmed().isEmpty())
        {
            qCWarning(gamelogDatabaseLog) << "Cannot initialize a database with an empty path.";
            return false;
        }

        // A prior failed attempt should not leave a live handle on this manager.
        if(database_.isValid()) { closeDatabase(); }

        if(!openDatabase()) { return false; }

        if(!configureDatabase() || !runMigrations())
        {
            closeDatabase();
            return false;
        }

        initialized_ = true;
        return true;
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
        if(!commandLinePath.isEmpty())
        {
            if(commandLinePath == QStringLiteral(":memory:")) { return commandLinePath; }
            return QFileInfo{commandLinePath}.absoluteFilePath();
        }

        // Allow local development and test runs to redirect storage.
        if(const QString environmentPath = qEnvironmentVariable("GAMELOG_DATABASE_PATH"); !environmentPath.isEmpty())
        {
            if(environmentPath == QStringLiteral(":memory:")) { return environmentPath; }
            return QFileInfo{environmentPath}.absoluteFilePath();
        }

        // Fall back to the default per-user location.
        return defaultDatabasePath();
    }

    bool DatabaseManager::openDatabase()
    {
        if(connectionName_.trimmed().isEmpty())
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
        database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
        ownsConnection_ = true;
        database_.setDatabaseName(databasePath_);

        if(!database_.open())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to open database:" << database_.lastError().text();
            closeDatabase();
            return false;
        }

        return true;
    }

    bool DatabaseManager::configureDatabase()
    {
        // Foreign keys are required for the repository relationships.
        QSqlQuery query{database_};
        if(!query.exec(QStringLiteral("PRAGMA foreign_keys = ON")))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to enable foreign keys:" << query.lastError().text();
            return false;
        }

        // Keep SQLite from failing immediately when a second writer appears.
        if(!query.exec(QStringLiteral("PRAGMA busy_timeout = 5000")))
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

    void DatabaseManager::closeDatabase()
    {
        initialized_ = false;
        const QString connectionName = connectionName_;
        const bool removeOwnedConnection = ownsConnection_;
        ownsConnection_ = false;

        if(database_.isValid()) { database_.close(); }

        // All QSqlDatabase handles owned here must release the connection before
        // removeDatabase() is called. Never remove another owner's connection
        // merely because it uses the same requested name.
        database_ = QSqlDatabase{};
        if(removeOwnedConnection && !connectionName.isEmpty() && QSqlDatabase::contains(connectionName))
        {
            QSqlDatabase::removeDatabase(connectionName);
        }
    }
} // namespace gamelog::core::database
