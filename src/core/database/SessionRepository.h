#pragma once

#include <optional>
#include <vector>
#include <QtSql/qsqldatabase.h>
#include "domain/Session.h"

namespace gamelog::core::database
{
class SessionRepository
{
public:
    explicit SessionRepository(QSqlDatabase database);

    [[nodiscard]] std::optional<domain::Session> findActiveSession();
    [[nodiscard]] std::vector<domain::Session> listSessionsForGame(int gameId);

    bool insert(domain::Session& session);
    bool update(const domain::Session& session);
    bool remove(int sessionId);

private:
    QSqlDatabase database_;
};
} // namespace gamelog::core::database
