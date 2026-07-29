#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include <QString>

#include "database/DatabaseManager.h"
#include "process/ProcessSource.h"

namespace gamelog::agent
{

class AgentApplication
{
public:
    explicit AgentApplication(QString databasePath);

    void start();
    void stop();
    void checkForGames();

    [[nodiscard]] bool syncGamesWithDatabase();

private:
    bool m_running{false};
    bool m_databaseReady{false};

    std::unique_ptr<core::process::ProcessSource> m_processSource;

    std::unordered_set<std::string> m_trackedExecutables;

    core::database::DatabaseManager m_databaseManager;
};

} // namespace gamelog::agent