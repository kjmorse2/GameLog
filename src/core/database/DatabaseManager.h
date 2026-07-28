#pragma once

#include <QString>
#include <QSqlDatabase>

namespace gamelog::core::database
{

class DatabaseManager
{
public:
    explicit DatabaseManager(QString databasePath);

    /**
     * @brief Initializes the database connection and applies any necessary migrations.
     * @return True if the database was successfully initialized, false otherwise.
     */
    [[nodiscard]] bool initialize();

    /**
     * @brief Checks if the database connection is open.
     * @return True if the database connection is open, false otherwise.
     */
    [[nodiscard]] bool isOpen() const;

    /**
     * @brief Gets the QSqlDatabase object representing the database connection.
     * @return The QSqlDatabase object.
     */
    [[nodiscard]] QSqlDatabase database() const;

private:

    /**
     * @brief Opens the database connection.
     * @return The QSqlDatabase object representing the opened database connection. 
     */
    [[nodiscard]] bool openDatabase();

    /**
     * @brief Runs database migrations to update the schema to the latest version.
     * @return True if migrations were successfully applied, false otherwise.
     */
    [[nodiscard]] bool runMigrations();

    /**
     * @brief Gets the path to the database file.
     * @return The path to the database file.
     */
    [[nodiscard]] const QString& databasePath() const;

    /**
     * @brief The path to the database file.
     */
    QString databasePath_;

    /**
     * @brief The name of the database connection.
     */
    QString connectionName_;

    /**
     * @brief The QSqlDatabase object representing the database connection.
     */
    QSqlDatabase database_;
};

} // namespace gamelog::core::database