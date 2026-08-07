#pragma once

#include <optional>
#include <vector>

#include <QSqlDatabase>
#include <QString>

namespace gamelog::core::database {

    /**
     * @brief Metadata for one ordered schema migration.
     *
     * Add future migrations to DatabaseMigrator::knownMigrations() and the
     * migrations.qrc resource file. Version numbers must stay unique and ordered.
     */
    struct Migration
    {
        /**
         * @brief Monotonically increasing schema version.
         */
        int version;

        /**
         * @brief Short human-readable migration name.
         */
        QString name;

        /**
         * @brief Qt resource path for the SQL payload.
         */
        QString resourcePath;
    };

    /**
     * @brief Applies any schema migrations that have not yet been recorded.
     */
    class DatabaseMigrator
    {
    public:
        /**
         * @brief Uses an existing open database connection.
         */
        explicit DatabaseMigrator(const QSqlDatabase &database);

        /**
         * @brief Ensures the migration ledger exists and applies pending steps.
         * @return boolean describing success.
         */
        [[nodiscard]] bool applyPendingMigrations();

    private:
        /**
         * @brief Creates the migration history table if it is missing.
         * @return boolean describing if table exists.
         */
        [[nodiscard]] bool ensureMigrationTable();

        /**
         * @brief Reports whether a migration version is already recorded.
         * @param version to check if it is applied.
         * @return Optional boolean. False if not applied, true if applied, and null if version does not exist.
         */
        [[nodiscard]] std::optional<bool> isApplied(int version) const;

        /**
         * @brief Executes one migration inside a transaction and records it.
         * @param migration the Migration to apply
         * @return boolean describing success.
         */
        [[nodiscard]] bool applyMigration(const Migration &migration);

        /**
         * @brief Reads a SQL script from a Qt resource path.
         * @param resourcePath The path to the Qt resource containing the SQL.
         * @return The SQL script as a string, or std::nullopt if reading fails.
         */
        [[nodiscard]] static std::optional<QString> readMigration(const QString &resourcePath);

        /**
         * @brief Returns the compiled-in migration list in application order.
         */
        [[nodiscard]] static const std::vector<Migration> &knownMigrations();

        QSqlDatabase database_;
    };

} // namespace gamelog::core::database
