//
// Created by kj on 8/12/26.
//

#include "gui/live_window/LiveWindow.h"

#include <QTimer>

#include "application/GameLogRuntime.h"
#include "gui/game_card/GameCard.h"

#include "ui_livewindow.h"

namespace gamelog::gui {
    LiveWindow::LiveWindow(application::GameLogRuntime& runtime, QWidget *parent) :
        QMainWindow(parent), gameLogRuntime(runtime), ui(new Ui::LiveWindow)
    {
        ui->setupUi(this);
        connect(gameLogRuntime.getSessionService(), &application::services::SessionService::sessionStarted, this, &LiveWindow::onSessionStarted);
        clockTimer = new QTimer(this);
        clockTimer->setInterval(1000);
        currentTime = QTime(0, 0);
    }

    void LiveWindow::onSessionStarted(Game game)
    {
        std::optional<Session> session = gameLogRuntime.activeSession();
        QGridLayout *layout = ui->mainGridLayout;
        QDateTime startTime = session->startTimestamp;
        int activeGameId = session->gameId;
        std::optional<Game> activeGame = gameLogRuntime.getGameService()->findById(activeGameId).value();

        // Remove and delete the old widget
        layout->removeWidget(ui->cardWidget);
        delete ui->cardWidget;

        // Create and add the new widget
        ui->cardWidget = new GameCard(ui->centralwidget, activeGame.value());
        layout->addWidget(ui->cardWidget, 0, 0);  // Insert at position 0 (first)

        qint64 miliSecondsDiff = startTime.msecsTo(QDateTime::currentDateTimeUtc());
        currentTime = QTime(0, 0).addMSecs(miliSecondsDiff);
        connect(clockTimer, &QTimer::timeout, this, &LiveWindow::updateTimerText);
        clockTimer->start();
    }

    void LiveWindow::updateTimerText()
    {
        ui->timeLabel->setText(currentTime.toString());
        currentTime = currentTime.addSecs(1);
    }



    LiveWindow::~LiveWindow()
    {
        delete ui;
    }
} // namespace gamelog::gui
