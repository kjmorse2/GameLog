#pragma once

#include <QDateTime>
#include <QString>

namespace gamelog::core::domain
{
struct SessionDocument
{
    int sessionId{0};
    QString htmlContent;
    QDateTime lastSavedTimestamp;
};
} // namespace gamelog::core::domain
