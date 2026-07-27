#include <QApplication>

#include "application/GuiApplication.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("GameLog"));
    QApplication::setApplicationName(QStringLiteral("gamelog"));

    gamelog::gui::application::GuiApplication guiApplication;
    guiApplication.start();

    return app.exec();
}
