#pragma once

#include <QString>

namespace gamelog::core::database
{
class DatabaseManager
{
public:
    explicit DatabaseManager(QString databasePath);

    [[nodiscard]] bool initialize();
    [[nodiscard]] const QString &databasePath() const;

private:
    QString m_databasePath;
};
} // namespace gamelog::core::database
