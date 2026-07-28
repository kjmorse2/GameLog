#pragma once

#include <optional>
#include <vector>

#include <QSqlDatabase>

#include "domain/Session.h"

namespace gamelog::core::database
{

class SessionRepository
{
public:
    explicit SessionRepository(QSqlDatabase database);

    [[nodiscard]] std::optional<domain::Session>
    findActiveSession() const;

    [[nodiscard]] std::vector<domain::Session>
    listSessionsForGame(int gameId) const;

    bool insert(domain::Session& session);
    bool update(const domain::Session& session);
    bool remove(int sessionId);

private:
    QSqlDatabase database_;
};

} // namespace gamelog::core::database
