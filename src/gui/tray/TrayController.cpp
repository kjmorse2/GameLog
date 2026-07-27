#include "tray/TrayController.h"

#include <QLoggingCategory>

#include "logging/LoggingCategories.h"

namespace gamelog::gui::tray
{
void TrayController::initialize()
{
    // TODO: Add optional system tray integration when desktop support is validated.
    qCInfo(gamelogGuiLog) << "Tray initialization is not implemented.";
}

bool TrayController::isAvailable() const
{
    return false;
}
} // namespace gamelog::gui::tray
