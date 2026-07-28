#pragma once

#include <QSqlDatabase>
#include <QString>

namespace gamelog::core::database
{

class DatabaseManager
{
public:
    DatabaseManager(QString databasePath, QString connectionName);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&&) = delete;
    DatabaseManager& operator=(DatabaseManager&&) = delete;

    /**
     * Opens, configures, and migrates the database.
     */
    [[nodiscard]] bool initialize();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QSqlDatabase database() const;

private:
    [[nodiscard]] bool openDatabase();
    [[nodiscard]] bool configureDatabase();
    [[nodiscard]] bool runMigrations();

    QString databasePath_;
    QString connectionName_;
    QSqlDatabase database_;
};

} // namespace gamelog::core::database
