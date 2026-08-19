#include <QtTest/QtTest>

#include "database/DatabaseMigrator.h"
#include "fixtures/TestDatabaseFixture.h"

#include <vector>

#include <QDateTime>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

using gamelog::core::database::DatabaseMigrator;

namespace
{
    struct ExpectedMigration
    {
        int version;
        QString name;
    };

    const std::vector<ExpectedMigration> expectedMigrations{
        {1, QStringLiteral("initial_schema")}, {2, QStringLiteral("reconfig_session_documents_table")},
        {3, QStringLiteral("remove_format_from_session_documents")}, {4, QStringLiteral("artwork_path_to_has_artwork")}
    };

    /**
     * Executes one migration resource using the same statement splitting the
     * migrator applies, so staged databases match production exactly.
     */
    bool executeMigrationResource(const QSqlDatabase& database, const QString& resourcePath)
    {
        QFile file{resourcePath};
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) { return false; }

        const QString script = QString::fromUtf8(file.readAll());

        for(const QString& statement : script.split(QStringLiteral("-- statement-break"), Qt::SkipEmptyParts))
        {
            const QString trimmed = statement.trimmed();
            if(trimmed.isEmpty()) { continue; }

            QSqlQuery query{database};
            if(!query.exec(trimmed)) { return false; }
        }

        return true;
    }

    bool recordMigration(const QSqlDatabase& database, const int version, const QString& name)
    {
        QSqlQuery query{database};
        query.prepare(QStringLiteral("INSERT INTO schema_migrations (version, name, applied_at_utc) "
                                     "VALUES (:version, :name, :applied_at_utc)"));
        query.bindValue(QStringLiteral(":version"), version);
        query.bindValue(QStringLiteral(":name"), name);
        query.bindValue(QStringLiteral(":applied_at_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

        return query.exec();
    }

    bool createLedgerTable(const QSqlDatabase& database)
    {
        QSqlQuery query{database};
        return query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS schema_migrations (version INTEGER PRIMARY KEY, "
                                         "name TEXT NOT NULL UNIQUE, applied_at_utc TEXT NOT NULL)"));
    }

    bool objectExists(const QSqlDatabase& database, const QString& type, const QString& name)
    {
        QSqlQuery query{database};
        query.prepare(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = :type AND name = :name LIMIT 1"));
        query.bindValue(QStringLiteral(":type"), type);
        query.bindValue(QStringLiteral(":name"), name);

        return query.exec() && query.next();
    }
} // namespace

namespace
{
    class DatabaseMigratorTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        void cleanup();

        void applyPendingMigrations_recordsEveryCompiledMigration();

        void applyPendingMigrations_createsExpectedSchemaObjects();

        void applyPendingMigrations_isIdempotent();

        void applyPendingMigrations_failsForInvalidDatabase();

        void applyPendingMigrations_failsForClosedDatabase();

        void applyPendingMigrations_failsForUnknownLedgerVersion();

        void applyPendingMigrations_failsForLedgerNameMismatch();

        void applyPendingMigrations_mapsLegacyArtworkPathToHasArtwork();

    private:
        [[nodiscard]] bool stageDatabaseAtVersionThree() const;

        [[nodiscard]] int hasArtworkForTitle(const QString& title) const;

        QString databasePath_;
        QString connectionName_;
        QSqlDatabase database_;
    };
} // namespace

void DatabaseMigratorTest::init()
{
    QTest::failOnWarning();

    databasePath_ = gamelog::tests::fixtures::createFreshTestDatabasePath(QStringLiteral("database-migrator-%1").
                                                                          arg(QString::fromLatin1(QTest::currentTestFunction())));
    connectionName_ = gamelog::tests::fixtures::createUniqueConnectionName("database-migrator");

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(databasePath_);
    QVERIFY(database_.open());
}

void DatabaseMigratorTest::cleanup()
{
    if(database_.isValid())
    {
        database_.close();
        database_ = QSqlDatabase{};
    }

    if(QSqlDatabase::contains(connectionName_)) { QSqlDatabase::removeDatabase(connectionName_); }

    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath_);
}

bool DatabaseMigratorTest::stageDatabaseAtVersionThree() const
{
    // Reusing the real resource scripts keeps the staged schema identical to
    // production rather than duplicating DDL inside the test.
    const QStringList stagedResources{
        QStringLiteral(":/migrations/001_initial_schema.sql"),
        QStringLiteral(":/migrations/002_reconfig_session_documents.sql"),
        QStringLiteral(":/migrations/003_remove_format_session_documents.sql")
    };

    for(const QString& resourcePath : stagedResources)
    {
        if(!executeMigrationResource(database_, resourcePath)) { return false; }
    }

    if(!createLedgerTable(database_)) { return false; }

    for(std::size_t index = 0; index < 3; ++index)
    {
        const ExpectedMigration& migration = expectedMigrations[index];
        if(!recordMigration(database_, migration.version, migration.name)) { return false; }
    }

    return true;
}

int DatabaseMigratorTest::hasArtworkForTitle(const QString& title) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral("SELECT has_artwork FROM games WHERE title = :title"));
    query.bindValue(QStringLiteral(":title"), title);

    if(!query.exec() || !query.next()) { return -1; }
    if(query.value(0).isNull()) { return -2; }

    return query.value(0).toInt();
}

void DatabaseMigratorTest::applyPendingMigrations_recordsEveryCompiledMigration()
{
    DatabaseMigrator migrator{database_};
    QVERIFY(migrator.applyPendingMigrations());

    QSqlQuery query{database_};
    QVERIFY(query.exec(QStringLiteral("SELECT version, name FROM schema_migrations ORDER BY version")));

    for(const ExpectedMigration& expected : expectedMigrations)
    {
        QVERIFY(query.next());
        QCOMPARE(query.value(QStringLiteral("version")).toInt(), expected.version);
        QCOMPARE(query.value(QStringLiteral("name")).toString(), expected.name);
    }

    QVERIFY(!query.next());
}

void DatabaseMigratorTest::applyPendingMigrations_createsExpectedSchemaObjects()
{
    DatabaseMigrator migrator{database_};
    QVERIFY(migrator.applyPendingMigrations());

    QVERIFY(objectExists(database_, QStringLiteral("table"), QStringLiteral("games")));
    QVERIFY(objectExists(database_, QStringLiteral("table"), QStringLiteral("sessions")));
    QVERIFY(objectExists(database_, QStringLiteral("table"), QStringLiteral("session_documents")));
    QVERIFY(objectExists(database_, QStringLiteral("table"), QStringLiteral("schema_migrations")));
    QVERIFY(objectExists(database_, QStringLiteral("index"), QStringLiteral("one_active_session")));
    QVERIFY(objectExists(database_, QStringLiteral("index"), QStringLiteral("sessions_by_game_and_start")));
}

void DatabaseMigratorTest::applyPendingMigrations_isIdempotent()
{
    DatabaseMigrator first{database_};
    QVERIFY(first.applyPendingMigrations());

    DatabaseMigrator second{database_};
    QVERIFY(second.applyPendingMigrations());

    QSqlQuery query{database_};
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM schema_migrations")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), static_cast<int>(expectedMigrations.size()));
}

void DatabaseMigratorTest::applyPendingMigrations_failsForInvalidDatabase()
{
    const QSqlDatabase invalidDatabase;
    DatabaseMigrator migrator{invalidDatabase};

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*closed or invalid database.*"));
    QVERIFY(!migrator.applyPendingMigrations());
}

void DatabaseMigratorTest::applyPendingMigrations_failsForClosedDatabase()
{
    database_.close();

    DatabaseMigrator migrator{database_};

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*closed or invalid database.*"));
    QVERIFY(!migrator.applyPendingMigrations());

    QVERIFY(database_.open());
}

void DatabaseMigratorTest::applyPendingMigrations_failsForUnknownLedgerVersion()
{
    QVERIFY(createLedgerTable(database_));
    QVERIFY(recordMigration(database_, 999, QStringLiteral("from_the_future")));

    DatabaseMigrator migrator{database_};

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*unknown migration version 999.*"));
    QVERIFY(!migrator.applyPendingMigrations());

    // Startup must abort rather than partially migrating an incompatible schema.
    QVERIFY(!objectExists(database_, QStringLiteral("table"), QStringLiteral("games")));
}

void DatabaseMigratorTest::applyPendingMigrations_failsForLedgerNameMismatch()
{
    QVERIFY(createLedgerTable(database_));
    QVERIFY(recordMigration(database_, 1, QStringLiteral("initial_schema_renamed")));

    DatabaseMigrator migrator{database_};

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*name mismatch for version 1.*"));
    QVERIFY(!migrator.applyPendingMigrations());
    QVERIFY(!objectExists(database_, QStringLiteral("table"), QStringLiteral("games")));
}

void DatabaseMigratorTest::applyPendingMigrations_mapsLegacyArtworkPathToHasArtwork()
{
    // The migrator constructor is what registers the compiled-in resources.
    DatabaseMigrator resourceOwner{database_};
    QVERIFY(stageDatabaseAtVersionThree());

    const std::vector<QPair<QString, QVariant>> legacyRows{
        {QStringLiteral("Null Path"), QVariant{}}, {QStringLiteral("Empty Path"), QVariant{QStringLiteral("")}},
        {QStringLiteral("Spaces Path"), QVariant{QStringLiteral("   ")}},
        {QStringLiteral("Whitespace Mix Path"), QVariant{QStringLiteral("\t\n\v\f\r ")}},
        {QStringLiteral("Real Path"), QVariant{QStringLiteral("/home/player/artwork/cover.jpg")}},
        {QStringLiteral("Padded Real Path"), QVariant{QStringLiteral("  /home/player/artwork/cover.jpg  ")}}
    };

    for(const auto& [title, artworkPath] : legacyRows)
    {
        QSqlQuery insert{database_};
        insert.prepare(QStringLiteral("INSERT INTO games (title, artwork_path) VALUES (:title, :artwork_path)"));
        insert.bindValue(QStringLiteral(":title"), title);
        insert.bindValue(QStringLiteral(":artwork_path"), artworkPath);
        QVERIFY(insert.exec());
    }

    QVERIFY(resourceOwner.applyPendingMigrations());

    QCOMPARE(hasArtworkForTitle(QStringLiteral("Null Path")), 0);
    QCOMPARE(hasArtworkForTitle(QStringLiteral("Empty Path")), 0);
    QCOMPARE(hasArtworkForTitle(QStringLiteral("Spaces Path")), 0);
    QCOMPARE(hasArtworkForTitle(QStringLiteral("Whitespace Mix Path")), 0);
    QCOMPARE(hasArtworkForTitle(QStringLiteral("Real Path")), 1);
    QCOMPARE(hasArtworkForTitle(QStringLiteral("Padded Real Path")), 1);

    // The legacy column is gone and only migration 004 was newly recorded.
    QVERIFY(!objectExists(database_, QStringLiteral("table"), QStringLiteral("artwork_path")));

    QSqlQuery ledger{database_};
    QVERIFY(ledger.exec(QStringLiteral("SELECT COUNT(*) FROM schema_migrations")));
    QVERIFY(ledger.next());
    QCOMPARE(ledger.value(0).toInt(), static_cast<int>(expectedMigrations.size()));
}

QTEST_GUILESS_MAIN(DatabaseMigratorTest)

#include "DatabaseMigratorTest.moc"
