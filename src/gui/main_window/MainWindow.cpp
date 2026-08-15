#include "MainWindow.h"

#include <QLabel>
#include <QObject>
#include <QWidget>

#include "gui/calendar/CalendarView.h"
#include "gui/library/LibraryView.h"
#include "application/GameLogRuntime.h"
#include "application/services/SessionService.h"

#include "ui_mainwindow.h"

static void initializeGUIResources()
{
    static const bool initialized = []
    {
        Q_INIT_RESOURCE(images);
        return true;
    }();

    static_cast<void>(initialized);
}

namespace gamelog::gui
{
    MainWindow::MainWindow(application::GameLogRuntime& runtime, QWidget* parent): QMainWindow{parent},
                                                                                   ui(new Ui::MainWindow),
                                                                                   runtime_{runtime}
    {
        initializeGUIResources();
        ui->setupUi(this);
        auto* libraryViewWidget = new LibraryView{ui->libraryTab, &runtime};
        ui->libraryTabLayout->addWidget(libraryViewWidget);

        ui->calanderTabLayout->addWidget(new CalendarView{ui->calanderTab, runtime.getGameService(), runtime.getSessionService()});

        statusActiveLabel_ = new QLabel{ui->statusBar};
        statusTitleLabel_ = new QLabel{ui->statusBar};
        statusTimeLabel_ = new QLabel{ui->statusBar};

        ui->statusBar->addPermanentWidget(statusActiveLabel_);
        ui->statusBar->addPermanentWidget(statusTitleLabel_);
        ui->statusBar->addPermanentWidget(statusTimeLabel_);

        const auto sessionService = runtime_.getSessionService();

        connect(sessionService, &application::services::SessionService::sessionStarted, this, &MainWindow::onSessionStarted);
        connect(runtime_.getSessionService(), &application::services::SessionService::sessionStopped, this, &MainWindow::onSessionEnded);
        onSessionEnded();
    }

    MainWindow::~MainWindow()
    {
        delete ui;
    }

    void MainWindow::onSessionStarted(const core::domain::Game& game)
    {
        statusActiveLabel_->setText("Active");
        statusTitleLabel_->setText(game.title);
        statusTimeLabel_->setText("00:00");
    }

    void MainWindow::onSessionEnded()
    {
        statusActiveLabel_->setText("Inactive");
        statusTitleLabel_->setText("None");
        statusTimeLabel_->setText("00:00");
    }
} // namespace gamelog::gui
