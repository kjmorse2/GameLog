#include <QtTest/QtTest>

#include "fixtures/TestDatabaseFixture.h"
#include "database/DatabaseManager.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>

using gamelog::core::database::DatabaseManager;

namespace
{
    bool tableExists(QSqlDatabase database, const QString& tableName)
    {
        QSqlQuery query{database};
        query.prepare(
                      R"(
                    SELECT 1
                    FROM sqlite_master
                    WHERE type = 'table' AND name = :table_name
                    LIMIT 1
                )"
                     );
        query.bindValue(":table_name", tableName);

        return query.exec() && query.next();
    }
} // namespace

class DatabaseManagerTest:public QObject
{
    Q_OBJECT

private
    slots:



    void init();

    void cleanup();

    void initialize_opensDatabaseAndRunsMigrations();

    void initialize_failsForEmptyConnectionName();

    void initialize_failsWhenConnectionAlreadyExists();

    void isOpen_isFalseBeforeInitialize();

    void database_isInvalidBeforeInitialize();

    void database_isValidAndOpenAfterInitialize();

    void defaultDatabasePath_returnsSQLitePath();

    void resolveDatabasePath_prefersCommandLinePath();

    void resolveDatabasePath_usesEnvironmentPathWithoutCommandLinePath();

    void resolveDatabasePath_fallsBackToDefaultPath();

private:
    QByteArray originalEnvironmentValue_;
    bool hadOriginalEnvironmentValue_ = false;
};

void DatabaseManagerTest::init()
{
    hadOriginalEnvironmentValue_ = qEnvironmentVariableIsSet("GAMELOG_DATABASE_PATH");

    if(hadOriginalEnvironmentValue_)
    {
        originalEnvironmentValue_ = qgetenv("GAMELOG_DATABASE_PATH");
    }
    else
    {
        originalEnvironmentValue_.clear();
    }

    qunsetenv("GAMELOG_DATABASE_PATH");
}

void DatabaseManagerTest::cleanup()
{
    if(hadOriginalEnvironmentValue_)
    {
        qputenv("GAMELOG_DATABASE_PATH", originalEnvironmentValue_);
    }
    else
    {
        qunsetenv("GAMELOG_DATABASE_PATH");
    }
}

void DatabaseManagerTest::initialize_opensDatabaseAndRunsMigrations()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("initialize-success");
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("initialize-success");

    {
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
        QCOMPARE(query.value(0).toInt(), 3);
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

void DatabaseManagerTest::isOpen_isFalseBeforeInitialize()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("is-open-before-initialize");
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("is-open-before-initialize");

    DatabaseManager manager{databasePath, connectionName};
    QVERIFY(!manager.isOpen());

    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::database_isInvalidBeforeInitialize()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("database-before-initialize");
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("database-before-initialize");

    DatabaseManager manager{databasePath, connectionName};
    QVERIFY(!manager.database().isValid());

    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath);
}

void DatabaseManagerTest::database_isValidAndOpenAfterInitialize()
{
    const QString databasePath = gamelog::tests::fixtures::createFreshTestDatabasePath("database-after-initialize");
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("database-after-initialize");

    {
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
