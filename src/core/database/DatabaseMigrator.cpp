#include "database/DatabaseMigrator.h"

#include "logging/LoggingCategories.h"

#include <algorithm>

#include <QDateTime>
#include <QFile>
#include <QResource>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

// The migration files are compiled into gamelog-core through migrations.qrc.
// Since gamelog-core is a static library, explicitly referencing the generated
// resource initializer ensures the linker includes the resource object.
static void initializeMigrationResources()
{
    static const bool initialized = []
    {
        Q_INIT_RESOURCE(migrations);
        return true;
    }();

    static_cast<void>(initialized);
}

namespace gamelog::core::database
{
    DatabaseMigrator::DatabaseMigrator(const QSqlDatabase& database) : database_{database}
    {
        initializeMigrationResources();
    }

    bool DatabaseMigrator::applyPendingMigrations()
    {
        // The migrator only operates on a live connection.
        if(!database_.isValid() || !database_.isOpen())
        {
            qCWarning(gamelogDatabaseLog) << "Cannot run migrations on a closed or invalid database.";
            return false;
        }

        // Make sure the ledger exists before we inspect or record anything.
        if(!ensureMigrationTable()) { return false; }

        // Read the ledger once; the apply loop below is a lookup, not a query.
        const std::optional<QHash<int, QString>> appliedMigrations = readAndValidateMigrationLedger();
        if(!appliedMigrations.has_value()) { return false; }

        // Apply each compiled-in migration in order.
        for(const Migration& migration : knownMigrations())
        {
            if(appliedMigrations->contains(migration.version)) { continue; }

            if(!applyMigration(migration)) { return false; }
        }

        return true;
    }

    bool DatabaseMigrator::ensureMigrationTable()
    {
        // Keep the migration ledger as a tiny, dependency-free table.
        QSqlQuery query{database_};
        if(!query.exec(R"(
                CREATE TABLE IF NOT EXISTS schema_migrations
                (
                    version INTEGER PRIMARY KEY,
                    name TEXT NOT NULL UNIQUE,
                    applied_at_utc TEXT NOT NULL
                )
            )"))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to create schema_migrations table:" << query.lastError().text();
            return false;
        }

        return true;
    }

    std::optional<QHash<int, QString>> DatabaseMigrator::readAndValidateMigrationLedger() const
    {
        QSqlQuery query{database_};
        if(!query.exec(QStringLiteral("SELECT version, name FROM schema_migrations ORDER BY version")))
        {
            qCWarning(gamelogDatabaseLog) << "Failed to validate applied migrations:" << query.lastError().text();
            return std::nullopt;
        }

        const auto& migrations = knownMigrations();
        QHash<int, QString> applied;

        while(query.next())
        {
            const int version = query.value(QStringLiteral("version")).toInt();
            const QString name = query.value(QStringLiteral("name")).toString();

            const auto known = std::find_if(migrations.begin(),
                                            migrations.end(),
                                            [version](const Migration& migration)
                                            {
                                                return migration.version == version;
                                            });

            if(known == migrations.end())
            {
                qCWarning(gamelogDatabaseLog) << "Database contains unknown migration version" << version <<
                    "and may use a newer incompatible schema.";
                return std::nullopt;
            }

            if(known->name != name)
            {
                qCWarning(gamelogDatabaseLog) << "Migration ledger name mismatch for version" << version << ": expected"
                    << known->name << "but found" << name;
                return std::nullopt;
            }

            applied.insert(version, name);
        }

        return applied;
    }

    bool DatabaseMigrator::applyMigration(const Migration& migration)
    {
        // Read the SQL script first so we can fail before opening the transaction.
        const std::optional<QString> sql = readMigration(migration.resourcePath);
        if(!sql)
        {
            qCWarning(gamelogDatabaseLog) << "Unable to read migration:" << migration.resourcePath;
            return false;
        }

        if(!database_.transaction())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to begin migration transaction:" << database_.lastError().text();
            return false;
        }

        const QStringList statements = sql->split(QStringLiteral("-- statement-break"), Qt::SkipEmptyParts);

        // Execute the script in discrete chunks so future migrations can bundle
        // multiple statements without depending on SQLite semicolon parsing.
        for(const QString& statement : statements)
        {
            const QString trimmed = statement.trimmed();
            if(trimmed.isEmpty()) { continue; }

            QSqlQuery query{database_};
            if(!query.exec(trimmed))
            {
                qCWarning(gamelogDatabaseLog) << "Migration failed:" << migration.version << migration.name << query.
                    lastError().text();
                database_.rollback();
                return false;
            }
        }

        QSqlQuery record{database_};
        record.prepare(R"(
            INSERT INTO schema_migrations
                (version, name, applied_at_utc)
            VALUES
                (:version, :name, :applied_at_utc)
        )");
        record.bindValue(QStringLiteral(":version"), migration.version);
        record.bindValue(QStringLiteral(":name"), migration.name);
        record.bindValue(QStringLiteral(":applied_at_utc"),
                         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

        if(!record.exec())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to record migration:" << migration.version << migration.name <<
                record.lastError().text();
            database_.rollback();
            return false;
        }

        if(!database_.commit())
        {
            qCWarning(gamelogDatabaseLog) << "Failed to commit migration:" << migration.version << migration.name <<
                database_.lastError().text();
            database_.rollback();
            return false;
        }

        qCInfo(gamelogDatabaseLog) << "Applied database migration:" << migration.version << migration.name;
        return true;
    }

    std::optional<QString> DatabaseMigrator::readMigration(const QString& resourcePath)
    {
        // Resource-backed scripts stay in sync with the compiled binary.
        QFile file{resourcePath};
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) { return std::nullopt; }

        return QString::fromUtf8(file.readAll());
    }

    const std::vector<Migration>& DatabaseMigrator::knownMigrations()
    {
        // Keep this list ordered so migration application stays deterministic.
        static const std::vector<Migration> migrations{
            {
                .version = 1, .name = QStringLiteral("initial_schema"),
                .resourcePath = QStringLiteral(":/migrations/001_initial_schema.sql")
            },
            {
                .version = 2, .name = QStringLiteral("reconfig_session_documents_table"),
                .resourcePath = QStringLiteral(":/migrations/002_reconfig_session_documents.sql")
            },
            {
                .version = 3, .name = QStringLiteral("remove_format_from_session_documents"),
                .resourcePath = QStringLiteral(":/migrations/003_remove_format_session_documents.sql")
            },
            {
                .version = 4, .name = QStringLiteral("artwork_path_to_has_artwork"),
                .resourcePath = QStringLiteral(":/migrations/004_artwork_path_to_has_artwork.sql")
            }
        };

        return migrations;
    }
} // namespace gamelog::core::database
