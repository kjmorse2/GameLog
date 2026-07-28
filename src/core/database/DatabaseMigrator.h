#pragma once

#include <optional>
#include <vector>

#include <QSqlDatabase>
#include <QString>

namespace gamelog::core::database
{

/**
 * Metadata describing one ordered schema migration.
 *
 * Add future migrations to DatabaseMigrator::knownMigrations() and to
 * migrations.qrc. The migration version must be unique and increasing.
 */
struct Migration
{
    int version;
    QString name;
    QString resourcePath;
};

class DatabaseMigrator
{
public:
    explicit DatabaseMigrator(QSqlDatabase database);

    /**
     * Creates the migration-history table and applies every migration that
     * has not previously been recorded.
     */
    [[nodiscard]] bool applyPendingMigrations();

private:
    [[nodiscard]] bool ensureMigrationTable();
    [[nodiscard]] std::optional<bool> isApplied(int version) const;
    [[nodiscard]] bool applyMigration(const Migration& migration);

    [[nodiscard]] static std::optional<QString> readMigration(const QString& resourcePath);

    [[nodiscard]] static const std::vector<Migration>& knownMigrations();

    QSqlDatabase database_;
};

} // namespace gamelog::core::database
