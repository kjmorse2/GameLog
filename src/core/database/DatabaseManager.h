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

        DatabaseManager(const DatabaseManager &) = delete;
        DatabaseManager &operator=(const DatabaseManager &) = delete;
        DatabaseManager(DatabaseManager &&) = delete;
        DatabaseManager &operator=(DatabaseManager &&) = delete;

        /**
         * @brief Opens the database, applies PRAGMA settings, and runs migrations.
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
         * @brief Returns the default persistent database path.
         *
         * Creates the containing directory if necessary.
         */
        [[nodiscard]]
        static QString defaultDatabasePath();

        /**
         * @brief Resolves the database path from CLI, environment, or defaults.
         *
         * Precedence:
         * 1. Explicit command-line path
         * 2. GAMELOG_DATABASE_PATH environment variable
         * 3. QStandardPaths application-data location
         */
        [[nodiscard]] static QString
        resolveDatabasePath(const QString &commandLinePath = {});

    private:
        /**
         * @brief Adds the connection to Qt and opens the underlying file.
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

        QString databasePath_;
        QString connectionName_;
        QSqlDatabase database_;
    };

} // namespace gamelog::core::database
