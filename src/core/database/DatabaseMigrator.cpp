#include "database/DatabaseMigrator.h"

#include "logging/LoggingCategories.h"

#include <utility>

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
    static const bool initialized = [] {
        Q_INIT_RESOURCE(migrations);
        return true;
    }();

    static_cast<void>(initialized);
}

namespace gamelog::core::database
{

DatabaseMigrator::DatabaseMigrator(QSqlDatabase database)
    : database_{std::move(database)}
{
    initializeMigrationResources();
}

bool DatabaseMigrator::applyPendingMigrations()
{
    if (!database_.isValid() || !database_.isOpen())
    {
        qCWarning(gamelogDatabaseLog)
            << "Cannot run migrations on a closed or invalid database.";
        return false;
    }

    if (!ensureMigrationTable())
    {
        return false;
    }

    for (const Migration& migration : knownMigrations())
    {
        const std::optional<bool> applied =
            isApplied(migration.version);

        if (!applied.has_value())
        {
            return false;
        }

        if (*applied)
        {
            continue;
        }

        if (!applyMigration(migration))
        {
            return false;
        }
    }

    return true;
}

bool DatabaseMigrator::ensureMigrationTable()
{
    QSqlQuery query{database_};

    if (!query.exec(
            R"(
                CREATE TABLE IF NOT EXISTS schema_migrations
                (
                    version INTEGER PRIMARY KEY,
                    name TEXT NOT NULL UNIQUE,
                    applied_at_utc TEXT NOT NULL
                )
            )"))
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to create schema_migrations table:"
            << query.lastError().text();

        return false;
    }

    return true;
}

std::optional<bool>
DatabaseMigrator::isApplied(int version) const
{
    QSqlQuery query{database_};
    query.prepare(
        R"(
            SELECT 1
            FROM schema_migrations
            WHERE version = :version
            LIMIT 1
        )"
    );
    query.bindValue(":version", version);

    if (!query.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to inspect applied migrations:"
            << query.lastError().text();

        return std::nullopt;
    }

    return query.next();
}

bool DatabaseMigrator::applyMigration(
    const Migration& migration
)
{
    const std::optional<QString> sql =
        readMigration(migration.resourcePath);

    if (!sql.has_value())
    {
        qCWarning(gamelogDatabaseLog)
            << "Unable to read migration:"
            << migration.resourcePath;

        return false;
    }

    if (!database_.transaction())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to begin migration transaction:"
            << database_.lastError().text();

        return false;
    }

    const QStringList statements = sql->split(
        "-- statement-break",
        Qt::SkipEmptyParts
    );

    for (const QString& statement : statements)
    {
        const QString trimmed = statement.trimmed();

        if (trimmed.isEmpty())
        {
            continue;
        }

        QSqlQuery query{database_};

        if (!query.exec(trimmed))
        {
            qCWarning(gamelogDatabaseLog)
                << "Migration failed:"
                << migration.version
                << migration.name
                << query.lastError().text();

            database_.rollback();
            return false;
        }
    }

    QSqlQuery record{database_};
    record.prepare(
        R"(
            INSERT INTO schema_migrations
                (version, name, applied_at_utc)
            VALUES
                (:version, :name, :applied_at_utc)
        )"
    );
    record.bindValue(":version", migration.version);
    record.bindValue(":name", migration.name);
    record.bindValue(
        ":applied_at_utc",
        QDateTime::currentDateTimeUtc()
            .toString(Qt::ISODateWithMs)
    );

    if (!record.exec())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to record migration:"
            << migration.version
            << migration.name
            << record.lastError().text();

        database_.rollback();
        return false;
    }

    if (!database_.commit())
    {
        qCWarning(gamelogDatabaseLog)
            << "Failed to commit migration:"
            << migration.version
            << migration.name
            << database_.lastError().text();

        database_.rollback();
        return false;
    }

    qCInfo(gamelogDatabaseLog)
        << "Applied database migration:"
        << migration.version
        << migration.name;

    return true;
}

std::optional<QString>
DatabaseMigrator::readMigration(
    const QString& resourcePath
)
{
    QFile file{resourcePath};

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        return std::nullopt;
    }

    return QString::fromUtf8(file.readAll());
}

const std::vector<Migration>&
DatabaseMigrator::knownMigrations()
{
    static const std::vector<Migration> migrations{
        {
            1,
            "initial_schema",
            ":/migrations/001_initial_schema.sql"
        }
    };

    return migrations;
}

} // namespace gamelog::core::database
