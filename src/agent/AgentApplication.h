#pragma once

namespace gamelog::agent
{
class AgentApplication
{
public:
    void start();
    void stop();

private:
    bool m_running{false};
};
} // namespace gamelog::agent
