#include "gui/MainWindow.h"

#include "application/GameLogRuntime.h"
#include "library/LibraryView.h"
#include "calander/CalanderView.h"

#include <QWidget>

#include "ui_mainwindow.h"

static void initializeGUIResources()
{
    static const bool initialized = [] {
        Q_INIT_RESOURCE(images);
        return true;
    }();

    static_cast<void>(initialized);
}
namespace gamelog::gui {

MainWindow::MainWindow(application::GameLogRuntime &runtime, QWidget *parent):
    QMainWindow{parent},
    ui(new Ui::MainWindow),
    runtime_{runtime}
{
    initializeGUIResources();
    ui->setupUi(this);
    auto *libraryViewWidget = new LibraryView{ui->libraryTab, runtime.getGameService()};
    ui->libraryTabLayout->addWidget(libraryViewWidget);

    auto *calanderViewWidget = new CalanderView{ui->calanderTab};
    ui->calanderTabLayout->addWidget(calanderViewWidget);
}

    MainWindow::~MainWindow()
{
    delete ui;
}


} // namespace gamelog::gui
