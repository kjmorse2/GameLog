#pragma once

#include "main_window/MainWindow.h"
#include "tray/TrayController.h"

namespace gamelog::gui::application
{
class GuiApplication
{
public:
    void start();

private:
    main_window::MainWindow m_mainWindow;
    tray::TrayController m_trayController;
};
} // namespace gamelog::gui::application
