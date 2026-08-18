//
// Created by kj on 8/12/26.
//

#include "gui/live_window/LiveWindow.h"

#include "application/GameLogRuntime.h"
#include "gui/game_card/GameCard.h"
#include "ui_livewindow.h"

#include <algorithm>

#include <QTimer>

namespace gamelog::gui
{
    LiveWindow::LiveWindow(application::GameLogRuntime& gameLogRuntime, QWidget* parent)
        : QMainWindow{parent},
          ui{new Ui::LiveWindow},
          gameLogRuntime{gameLogRuntime}
    {
        ui->setupUi(this);

        connect(gameLogRuntime.getSessionService(),
                &application::services::SessionService::sessionStarted,
                this,
                &LiveWindow::onSessionStarted);
        connect(gameLogRuntime.getSessionService(),
                &application::services::SessionService::sessionStopped,
                this,
                &LiveWindow::onSessionFinished);

        clockTimer = new QTimer{this};
        clockTimer->setInterval(1000);
        connect(clockTimer, &QTimer::timeout, this, &LiveWindow::updateTimerText);
        currentTime = QTime{0, 0};
    }

    LiveWindow::~LiveWindow() { delete ui; }

    void LiveWindow::onSessionStarted(const core::domain::Game& game)
    {
        QGridLayout* layout = ui->mainGridLayout;

        // Remove and delete the old game card widget.
        layout->removeWidget(ui->cardWidget);
        delete ui->cardWidget;

        // Create and add the new widget.
        ui->cardWidget = new GameCard{ui->centralwidget, game, gameLogRuntime.getArtworkService()};
        layout->addWidget(ui->cardWidget, 0, 0); // Insert at position 0 (first).

        // Set up the timer from the persisted active-session start.
        if(const auto session = gameLogRuntime.getSessionService()->findActiveSession())
        {
            const qint64 milliseconds = std::max<qint64>(0,
                                                         session->startTimestamp.
                                                                  msecsTo(QDateTime::currentDateTimeUtc()));
            currentTime = QTime{0, 0}.addMSecs(milliseconds);
        }
        else { currentTime = QTime{0, 0}; }
        clockTimer->start();

        // Enable note taking.
        ui->textEditor->setAbleToEdit(true);
    }

    void LiveWindow::onSessionFinished(core::domain::Session completedSession)
    {
        clockTimer->stop();

        QGridLayout* layout = ui->mainGridLayout;

        // Remove and delete the old game card widget.
        layout->removeWidget(ui->cardWidget);
        delete ui->cardWidget;

        // Create and add the empty widget.
        ui->cardWidget = new GameCard{ui->centralwidget, {}, gameLogRuntime.getArtworkService()};
        layout->addWidget(ui->cardWidget, 0, 0); // Insert at position 0 (first).

        ui->timeLabel->setText(QStringLiteral("Session Completed\n 00:00:00"));
        ui->textEditor->setAbleToEdit(false);

        completedSession.notes = ui->textEditor->getMarkdown();
        if(!gameLogRuntime.getSessionService()->updateSession(completedSession))
        {
            qWarning() << "Failed to update session notes for session id:" << completedSession.id;
        }
    }

    void LiveWindow::updateTimerText()
    {
        ui->timeLabel->setText(currentTime.toString());
        currentTime = currentTime.addSecs(1);
    }
} // namespace gamelog::gui
