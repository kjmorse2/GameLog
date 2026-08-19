#include <QtTest/QtTest>

#include "database/DatabaseManager.h"
#include "fixtures/TestDatabaseFixture.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>

using gamelog::core::database::DatabaseManager;

namespace
{
    bool tableExists(QSqlDatabase database, const QString& tableName)
    {
        QSqlQuery query{database};
        query.prepare(R"(
                    SELECT 1
                    FROM sqlite_master
                    WHERE type = 'table' AND name = :table_name
                    LIMIT 1
                )");
        query.bindValue(":table_name", tableName);

        return query.exec() && query.next();
    }
} // namespace

namespace
{
    class DatabaseManagerTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        void cleanup();

        static void initialize_opensDatabaseAndRunsMigrations();

        static void initialize_failsForEmptyConnectionName();

        static void initialize_failsWhenConnectionAlreadyExists();

        static void initialize_isIdempotentWhenRepeated();

        static void initialize_failsForBlankDatabasePaths();

        static void initialize_acceptsExplicitInMemoryDatabase();

        static void initialize_leavesNoHalfOpenStateAfterFailure();

        static void isOpen_isFalseBeforeInitialize();

        static void database_isInvalidBeforeInitialize();

        static void database_isValidAndOpenAfterInitialize();

        static void defaultDatabasePath_returnsSQLitePath();

        static void resolveDatabasePath_prefersCommandLinePath();

        static void resolveDatabasePath_usesEnvironmentPathWithoutCommandLinePath();

        static void resolveDatabasePath_fallsBackToDefaultPath();

    private:
        QByteArray originalEnvironmentValue_;
        bool hadOriginalEnvironmentValue_ = false;
    };
}

void DatabaseManagerTest::init()
{
    hadOriginalEnvironmentValue_ = qEnvironmentVariableIsSet("GAMELOG_DATABASE_PATH");

    if(hadOriginalEnvironmentValue_) { originalEnvironmentValue_ = qgetenv("GAMELOG_DATABASE_PATH"); }
    else { originalEnvironmentValue_.clear(); }

    qunsetenv("GAMELOG_DATABASE_PATH");
}

void DatabaseManagerTest::cleanup()
{
    if(hadOriginalEnvironmentValue_) { qputenv("GAMELOG_DATABASE_PATH", originalEnvironmentValue_); }
    else { qunsetenv("GAMELOG_DATABASE_PATH"); }
}

void DatabaseManagerTest::initialize_opensDatabaseAndRunsMigrations()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("initialize-success");

    {
        const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("initialize-success");
        DatabaseManager manager{databasePath, connectionName};

        QVERIFY(manager.initialize());
        QVERIFY(manager.isOpen());
        QVERIFY(manager.database().isValid());
        QVERIFY(manager.database().isOpen());

        QVERIFY(tableExists(manager.database(), "games"));
        QVERIFY(tableExists(manager.database(), "sessions"));
        QVERIFY(tableExists(manager.database(), "session_documents"));
        QVERIFY(tableExists(manager.database(), "schema_migrations"));

        QSqlQuery query{manager.database()};
        QVERIFY(query.exec("SELECT COUNT(*) FROM schema_migrations"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 4);
    }

    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::initialize_failsForEmptyConnectionName()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("empty-connection-name");

    DatabaseManager manager{databasePath, ""};
    QVERIFY(!manager.initialize());
    QVERIFY(!manager.isOpen());

    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::initialize_failsWhenConnectionAlreadyExists()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("duplicate-connection");
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("duplicate-connection");

    QSqlDatabase existingConnection = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    existingConnection.setDatabaseName(databasePath);
    QVERIFY(existingConnection.open());

    DatabaseManager manager{databasePath, connectionName};
    QVERIFY(!manager.initialize());
    QVERIFY(!manager.isOpen());

    existingConnection.close();
    existingConnection = QSqlDatabase{};
    QSqlDatabase::removeDatabase(connectionName);
    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::initialize_isIdempotentWhenRepeated()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("initialize-idempotent");

    {
        const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("initialize-idempotent");
        DatabaseManager manager{databasePath, connectionName};

        QVERIFY(manager.initialize());
        const QSqlDatabase firstHandle = manager.database();

        // A successful repeated call is a no-op rather than a reopen.
        QVERIFY(manager.initialize());
        QVERIFY(manager.initialize());

        QVERIFY(manager.isOpen());
        QCOMPARE(manager.database().connectionName(), firstHandle.connectionName());

        QSqlQuery query{manager.database()};
        QVERIFY(query.exec("SELECT COUNT(*) FROM schema_migrations"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 4);
    }

    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::initialize_failsForBlankDatabasePaths()
{
    for(const QString& blankPath : {QString{""}, QString{" "}, QString{"   "}, QString{"\t\n"}})
    {
        const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("initialize-blank-path");
        DatabaseManager manager{blankPath, connectionName};

        QVERIFY(!manager.initialize());
        QVERIFY(!manager.isOpen());

        // A rejected path must not have claimed the Qt connection name.
        QVERIFY(!QSqlDatabase::contains(connectionName));
    }
}

void DatabaseManagerTest::initialize_acceptsExplicitInMemoryDatabase()
{
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("initialize-in-memory");
    DatabaseManager manager{":memory:", connectionName};

    QVERIFY(manager.initialize());
    QVERIFY(manager.isOpen());
    QVERIFY(tableExists(manager.database(), "games"));
    QVERIFY(tableExists(manager.database(), "sessions"));
}

void DatabaseManagerTest::initialize_leavesNoHalfOpenStateAfterFailure()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("initialize-half-open");
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("initialize-half-open");

    // A colliding connection owned by someone else makes initialize() fail.
    QSqlDatabase foreignConnection = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    foreignConnection.setDatabaseName(databasePath);
    QVERIFY(foreignConnection.open());

    {
        DatabaseManager manager{databasePath, connectionName};

        QVERIFY(!manager.initialize());
        QVERIFY(!manager.isOpen());
        QVERIFY(!manager.database().isValid());

        // The other owner's connection must survive the failed attempt, both
        // while the manager is alive and after it is destroyed.
        QVERIFY(QSqlDatabase::contains(connectionName));
        QVERIFY(QSqlDatabase::database(connectionName, false).isOpen());
    }

    QVERIFY(QSqlDatabase::contains(connectionName));
    QVERIFY(QSqlDatabase::database(connectionName, false).isOpen());

    foreignConnection.close();
    foreignConnection = QSqlDatabase{};
    QSqlDatabase::removeDatabase(connectionName);
    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::isOpen_isFalseBeforeInitialize()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("is-open-before-initialize");
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("is-open-before-initialize");

    const DatabaseManager manager{databasePath, connectionName};
    QVERIFY(!manager.isOpen());

    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::database_isInvalidBeforeInitialize()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("database-before-initialize");
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("database-before-initialize");

    const DatabaseManager manager{databasePath, connectionName};
    QVERIFY(!manager.database().isValid());

    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::database_isValidAndOpenAfterInitialize()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("database-after-initialize");

    {
        const QString connectionName =
            gamelog::tests::fixtures::createUniqueConnectionName("database-after-initialize");
        DatabaseManager manager{databasePath, connectionName};
        QVERIFY(manager.initialize());

        const QSqlDatabase database = manager.database();
        QVERIFY(database.isValid());
        QVERIFY(database.isOpen());
        QCOMPARE(database.connectionName(), connectionName);
    }

    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::defaultDatabasePath_returnsSQLitePath()
{
    const QString path = DatabaseManager::defaultDatabasePath();

    QVERIFY(!path.isEmpty());
    QVERIFY(path.endsWith("gamelog.sqlite"));
    QVERIFY(QFileInfo{path}.absoluteDir().exists());
}

void DatabaseManagerTest::resolveDatabasePath_prefersCommandLinePath()
{
    qputenv("GAMELOG_DATABASE_PATH", "/tmp/ignored-env-path.sqlite");

    const QString resolvedPath = DatabaseManager::resolveDatabasePath("relative/cli/path.sqlite");
    const QString expectedPath = QFileInfo{"relative/cli/path.sqlite"}.absoluteFilePath();

    QCOMPARE(resolvedPath, expectedPath);
}

void DatabaseManagerTest::resolveDatabasePath_usesEnvironmentPathWithoutCommandLinePath()
{
    qputenv("GAMELOG_DATABASE_PATH", "relative/env/path.sqlite");

    const QString resolvedPath = DatabaseManager::resolveDatabasePath();
    const QString expectedPath = QFileInfo{"relative/env/path.sqlite"}.absoluteFilePath();

    QCOMPARE(resolvedPath, expectedPath);
}

void DatabaseManagerTest::resolveDatabasePath_fallsBackToDefaultPath()
{
    qunsetenv("GAMELOG_DATABASE_PATH");

    const QString resolvedPath = DatabaseManager::resolveDatabasePath();
    const QString defaultPath = DatabaseManager::defaultDatabasePath();

    QCOMPARE(resolvedPath, defaultPath);
}

QTEST_GUILESS_MAIN(DatabaseManagerTest)

#include "DatabaseManagerTest.moc"
