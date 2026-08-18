#pragma once

#include <QSqlDatabase>
#include <QString>

namespace gamelog::core::database
{
    /**
     * @brief Owns one Qt SQL connection and applies the initial schema setup.
     */
    class DatabaseManager
    {
    public:
        /**
         * @brief Creates a manager for a specific database path and connection name.
         */
        DatabaseManager(QString databasePath, QString connectionName);

        ~DatabaseManager();

        DatabaseManager(const DatabaseManager&) = delete;
        DatabaseManager& operator=(const DatabaseManager&) = delete;
        DatabaseManager(DatabaseManager&&) = delete;
        DatabaseManager& operator=(DatabaseManager&&) = delete;

        /**
         * @brief Opens the database, applies PRAGMA settings, and runs migrations.
         *
         * A successful repeated call is idempotent. Empty or whitespace-only
         * database paths are rejected; the explicit SQLite value ":memory:" is
         * supported. Any failure after opening immediately releases the named
         * connection so the manager is not left partially initialized.
         * @return True when the manager is fully initialized.
         */
        [[nodiscard]] bool initialize();

        /**
         * @brief Returns whether the Qt SQL connection is currently open.
         */
        [[nodiscard]] bool isOpen() const;

        /**
         * @brief Returns the managed Qt SQL database handle.
         */
        [[nodiscard]] QSqlDatabase database() const;

        /**
         * @brief Returns the default persistent database path and creates its containing directory.
         */
        [[nodiscard]] static QString defaultDatabasePath();

        /**
         * @brief Resolves the database path from CLI, environment, or defaults.
         *
         * Precedence:
         * 1. Explicit command-line path
         * 2. GAMELOG_DATABASE_PATH environment variable
         * 3. QStandardPaths application-data location
         */
        [[nodiscard]] static QString resolveDatabasePath(const QString& commandLinePath = {});

    private:
        /**
         * @brief Adds the connection to Qt and opens the underlying SQLite database.
         */
        [[nodiscard]] bool openDatabase();

        /**
         * @brief Applies connection-level SQLite PRAGMAs.
         */
        [[nodiscard]] bool configureDatabase();

        /**
         * @brief Runs any pending schema migrations.
         */
        [[nodiscard]] bool runMigrations();

        /**
         * @brief Closes and removes the manager-owned named Qt connection.
         */
        void closeDatabase();

        /**
         * @brief File path or explicit SQLite database name.
         */
        QString databasePath_;

        /**
         * @brief Unique Qt SQL connection name owned by this manager.
         */
        QString connectionName_;

        /**
         * @brief The managed Qt SQL database handle.
         */
        QSqlDatabase database_;

        /**
         * @brief True after this manager has added the named Qt connection.
         */
        bool ownsConnection_{false};

        /**
         * @brief True only after configuration and migrations both succeed.
         */
        bool initialized_{false};
    };
} // namespace gamelog::core::database
