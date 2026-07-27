#include "settings/ApplicationSettings.h"

namespace gamelog::core::settings
{
ApplicationSettings::ApplicationSettings()
    : m_settings()
{
}

QString ApplicationSettings::applicationName() const
{
    return m_settings.value(QStringLiteral("application/name"), QStringLiteral("GameLog")).toString();
}

int ApplicationSettings::processPollingIntervalMs() const
{
    return m_settings.value(QStringLiteral("agent/pollingIntervalMs"), 5000).toInt();
}
} // namespace gamelog::core::settings
