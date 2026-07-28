#pragma once

#include <process/ProcessSource.h>

namespace gamelog::agent
{
class AgentApplication
{
public:
    void start();
    void stop();
    void checkForGames();

private:
    bool m_running{false};
    core::process::ProcessSource* m_processSource{nullptr};
};
} // namespace gamelog::agent
