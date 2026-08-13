//
// Created by kj on 8/12/26.
//

#include "gui/live_window/LiveWindow.h"

#include <QTimer>

#include "application/GameLogRuntime.h"
#include "gui/game_card/GameCard.h"

#include "ui_livewindow.h"

namespace gamelog::gui
{
    LiveWindow::LiveWindow(application::GameLogRuntime& runtime, QWidget* parent) : QMainWindow(parent), gameLogRuntime(runtime), ui(new Ui::LiveWindow)
    {
        ui->setupUi(this);
        connect(gameLogRuntime.getSessionService(), &application::services::SessionService::sessionStarted, this, &LiveWindow::onSessionStarted);
        connect(gameLogRuntime.getSessionService(), &application::services::SessionService::sessionStopped, this, &LiveWindow::onSessionFinished);
        clockTimer = new QTimer(this);
        clockTimer->setInterval(1000);
        currentTime = QTime(0, 0);
    }

    void LiveWindow::onSessionStarted(const Game& game)
    {

        QGridLayout* layout = ui->mainGridLayout;

        // Remove and delete the old game card widget
        layout->removeWidget(ui->cardWidget);
        delete ui->cardWidget;

        // Create and add the new widget
        ui->cardWidget = new GameCard(ui->centralwidget, game);
        layout->addWidget(ui->cardWidget, 0, 0); // Insert at position 0 (first)

        // Set up Timer
        std::optional<Session> session = gameLogRuntime.getSessionService()->findActiveSession();
        QDateTime startTime = session->startTimestamp;
        qint64 miliSecondsDiff = startTime.msecsTo(QDateTime::currentDateTimeUtc());
        currentTime = QTime(0, 0).addMSecs(miliSecondsDiff);
        connect(clockTimer, &QTimer::timeout, this, &LiveWindow::updateTimerText);
        clockTimer->start();

        // Enable note taking
        ui->textEditor->setAbleToEdit(true);
    }

    void LiveWindow::onSessionFinished(Session& completedSession)
    {
        clockTimer->stop();

        QGridLayout* layout = ui->mainGridLayout;
        // Remove and delete the old game card widget
        layout->removeWidget(ui->cardWidget);
        delete ui->cardWidget;

        // Create and add the new widget
        ui->cardWidget = new GameCard(ui->centralwidget, {});
        layout->addWidget(ui->cardWidget, 0, 0); // Insert at position 0 (first)

        ui->timeLabel->setText("Session Completed\n 00:00:00");

        ui->textEditor->setAbleToEdit(false);

        completedSession.notes = ui->textEditor->getMarkdown();
        if (!gameLogRuntime.getSessionService()->updateSession(completedSession))
        {
            qWarning() << "Failed to update session notes for session id:" << completedSession.id;
        }
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
