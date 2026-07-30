#pragma once

#include <QSqlDatabase>
#include <QString>

namespace gamelog::core::database {

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

        // Copy constructors
        DatabaseManager(const DatabaseManager &) = delete;
        DatabaseManager &operator=(const DatabaseManager &) = delete;
        DatabaseManager(DatabaseManager &&) = delete;
        DatabaseManager &operator=(DatabaseManager &&) = delete;

        /**
         * @brief Opens the database, applies PRAGMA settings, and runs migrations.
         * @return a boolean reporting success/failure.
         */
        [[nodiscard]] bool initialize();

        /**
         * @brief Returns whether the Qt SQL connection is currently open.
         * @return boolean reporting if connection is open.
         */
        [[nodiscard]] bool isOpen() const;

        /**
         * @brief Returns the managed Qt SQL database handle.
         * @return The database object connected to by this manager.
         */
        [[nodiscard]] QSqlDatabase database() const;

        /**
         * @brief Returns the default persistent database path, creates the containing directory if necessary.
         * @return The default database path.
         */
        [[nodiscard]] static QString defaultDatabasePath();

        /**
         * @brief Resolves the database path from CLI, environment, or defaults.
         * @return The resolved database path.
         *
         * Precedence:
         * 1. Explicit command-line path
         * 2. GAMELOG_DATABASE_PATH environment variable
         * 3. QStandardPaths application-data location
         */
        [[nodiscard]] static QString resolveDatabasePath(const QString &commandLinePath = {});

    private:
        /**
         * @brief Adds the connection to Qt and opens the underlying file.
         * @return boolean describing if successful.
         */
        [[nodiscard]] bool openDatabase();

        /**
         * @brief Applies connection-level SQLite PRAGMAs.
         * @return boolean describing if successful.
         */
        [[nodiscard]] bool configureDatabase();

        /**
         * @brief Runs any pending schema migrations.
         * @return boolean describing if successful.
         */
        [[nodiscard]] bool runMigrations();

        /**
         * @breif file path to database.
         */
        QString databasePath_;
        /**
         * Given name of connection.
         */
        QString connectionName_;

        /**
         * The managed Qt SQL database handle.
         */
        QSqlDatabase database_;
    };

} // namespace gamelog::core::database
