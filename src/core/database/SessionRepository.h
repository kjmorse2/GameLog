#pragma once

#include <optional>
#include <vector>

#include "domain/Session.h"

namespace gamelog::core::database
{
class SessionRepository
{
public:
    virtual ~SessionRepository() = default;

    [[nodiscard]] virtual std::optional<domain::Session> findActiveSession() = 0;
    [[nodiscard]] virtual std::vector<domain::Session> listSessionsForGame(int gameId) = 0;
    virtual bool save(const domain::Session &session) = 0;
};
} // namespace gamelog::core::database
