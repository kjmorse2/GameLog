#pragma once

#include <vector>

#include <QSqlDatabase>

#include "domain/Session.h"
#include "domain/query/SessionQuery.h"

namespace gamelog::core::database {

/**
 * Translates SessionQuery specifications into SQL and persists Session rows.
 */
class SessionRepository
{
public:
    explicit SessionRepository(const QSqlDatabase &database);

    [[nodiscard]] std::vector<domain::Session>
    query(const domain::query::SessionQuery &specification) const;

    [[nodiscard]] bool insert(domain::Session &session);
    [[nodiscard]] bool update(const domain::Session &session);
    [[nodiscard]] bool remove(int sessionId);

private:
    QSqlDatabase database_;
};

} // namespace gamelog::core::database
