#include "Session.h"

#include <stdexcept>

namespace gamelog::core::domain
{
    SessionSource sessionSourceFromString(const QString& sourceString)
    {
        if (sourceString == "automatic" || sourceString == "Automatic")
        {
            return SessionSource::Automatic;
        }
        if (sourceString == "manual" || sourceString == "Manual")
        {
            return SessionSource::Manual;
        }
        throw std::invalid_argument("Invalid session source string: " + sourceString.toStdString());
    }

    SessionStatus sessionStatusFromString(const QString& statusString)
    {
        if (statusString == "active" || statusString == "Active")
        {
            return SessionStatus::Active;
        }
        if (statusString == "completed" || statusString == "Completed")
        {
            return SessionStatus::Completed;
        }
        if (statusString == "interrupted" || statusString == "Interrupted")
        {
            return SessionStatus::Interrupted;
        }
        throw std::invalid_argument("Invalid session status string: " + statusString.toStdString());
    }
} // namespace gamelog::core::domain
