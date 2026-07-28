#pragma once

#include <QSqlDatabase>
#include <QString>

namespace gamelog::core::database
{

class DatabaseManager
{
public:
    DatabaseManager(QString databasePath, QString connectionName);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&&) = delete;
    DatabaseManager& operator=(DatabaseManager&&) = delete;

    /**
     * Opens, configures, and migrates the database.
     */
    [[nodiscard]] bool initialize();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QSqlDatabase database() const;

    /**
     * Returns the normal persistent database path.
     * Creates the containing directory if necessary.
     */
    [[nodiscard]]
    static QString defaultDatabasePath();

    /**
     * Resolves an override path or falls back to the normal path.
     *
     * Precedence:
     * 1. Explicit command-line path
     * 2. GAMELOG_DATABASE_PATH environment variable
     * 3. QStandardPaths application-data location
     */
    [[nodiscard]] static QString resolveDatabasePath(const QString& commandLinePath = {});

private:
    [[nodiscard]] bool openDatabase();
    [[nodiscard]] bool configureDatabase();
    [[nodiscard]] bool runMigrations();

    QString databasePath_;
    QString connectionName_;
    QSqlDatabase database_;
};

} // namespace gamelog::core::database
