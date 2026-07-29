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
 * @brief Applies any schema migrations that have not yet been recorded.
 */
class DatabaseMigrator
{
public:
    /**
     * @brief Uses an existing open database connection.
     */
    explicit DatabaseMigrator(QSqlDatabase database);

    /**
     * @brief Ensures the migration ledger exists and applies pending steps.
     */
    [[nodiscard]] bool applyPendingMigrations();

private:
    /**
     * @brief Creates the migration history table if it is missing.
     */
    [[nodiscard]] bool ensureMigrationTable();

    /**
     * @brief Reports whether a migration version is already recorded.
     */
    [[nodiscard]] std::optional<bool> isApplied(int version) const;

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
