#pragma once
#include <string>
#include <QSqlDatabase>
#include "database/DatabaseManager.h"
#include <unordered_set>

namespace gamelog::agent
{
class AgentApplication
{
public:
    AgentApplication(std::string databasePath = {});
    void start();
    void stop();
    void checkForGames();
    bool syncGamesWithDatabase();

private:
    bool m_running{false};
    core::process::ProcessSource* m_processSource{nullptr};
    std::unordered_set<std::string> m_trackedGames{};
    QSqlDatabase m_database;
    gamelog::core::database::DatabaseManager& m_databaseManager;
};
} // namespace gamelog::agent
