#include "application/GuiApplication.h"

#include <QLoggingCategory>

#include "logging/LoggingCategories.h"

namespace gamelog::gui::application
{
void GuiApplication::start()
{
    qCInfo(gamelogGuiLog) << "GameLog GUI started";
    m_trayController.initialize();
    m_mainWindow.show();
}
} // namespace gamelog::gui::application
