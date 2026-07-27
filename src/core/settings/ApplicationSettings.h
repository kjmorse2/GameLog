#pragma once

#include <QSettings>
#include <QString>

namespace gamelog::core::settings
{
class ApplicationSettings
{
public:
    ApplicationSettings();

    [[nodiscard]] QString applicationName() const;
    [[nodiscard]] int processPollingIntervalMs() const;

private:
    mutable QSettings m_settings;
};
} // namespace gamelog::core::settings
