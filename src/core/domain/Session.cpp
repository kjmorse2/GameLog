#include "Session.h"

namespace gamelog::core::domain
{
SessionSource sessionSourceFromString(const QString &sourceString)
{
    if (sourceString == "Automatic")
    {
        return SessionSource::Automatic;
    }
    else if (sourceString == "Manual")
    {
        return SessionSource::Manual;
    }
    else
    {
        throw std::invalid_argument("Invalid session source string: " + sourceString.toStdString());
    }
}

SessionStatus sessionStatusFromString(const QString &statusString)
{
    if (statusString == "Active")
    {
        return SessionStatus::Active;
    }
    else if (statusString == "Completed")
    {
        return SessionStatus::Completed;
    }
    else if (statusString == "Interrupted")
    {
        return SessionStatus::Interrupted;
    }
    else
    {
        throw std::invalid_argument("Invalid session status string: " + statusString.toStdString());
    }
}
} // namespace gamelog::core::domain