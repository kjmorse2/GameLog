#pragma once

#include <optional>
#include <vector>

#include <QSqlDatabase>
#include <QString>

namespace gamelog::core::database
{
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
     * @brief Validates the migration ledger and applies pending schema changes.
     */
    class DatabaseMigrator
    {
    public:
        /**
         * @brief Uses an existing open database connection.
         */
        explicit DatabaseMigrator(const QSqlDatabase& database);

        /**
         * @brief Ensures the ledger exists, validates it, and applies pending steps.
         *
         * A recorded version is accepted only when its name matches the compiled
         * migration. Unknown versions, including schemas newer than this binary,
         * are rejected as incompatible.
         * @return True if the database is compatible and fully migrated.
         */
        [[nodiscard]] bool applyPendingMigrations();

    private:
        /**
         * @brief Creates the migration history table if it is missing.
         */
        [[nodiscard]] bool ensureMigrationTable();

        /**
         * @brief Validates every ledger row against the compiled migration list.
         */
        [[nodiscard]] bool validateMigrationLedger() const;

        /**
         * @brief Reports whether the exact version/name pair is already recorded.
         * @return False if absent, true if present and matching, or std::nullopt on error/mismatch.
         */
        [[nodiscard]] std::optional<bool> isApplied(const Migration& migration) const;

        /**
         * @brief Executes one migration inside a transaction and records it.
         */
        [[nodiscard]] bool applyMigration(const Migration& migration);

        /**
         * @brief Reads a SQL script from a Qt resource path.
         */
        [[nodiscard]] static std::optional<QString> readMigration(const QString& resourcePath);

        /**
         * @brief Returns the compiled-in migration list in application order.
         */
        [[nodiscard]] static const std::vector<Migration>& knownMigrations();

        QSqlDatabase database_;
    };
} // namespace gamelog::core::database
