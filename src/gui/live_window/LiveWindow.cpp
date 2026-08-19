#include "gui/live_window/LiveWindow.h"

#include "application/GameLogRuntime.h"
#include "logging/LoggingCategories.h"
#include "gui/game_card/GameCard.h"
#include "ui_livewindow.h"

#include <algorithm>

#include <QTimer>

namespace gamelog::gui
{
    LiveWindow::LiveWindow(application::GameLogRuntime& gameLogRuntime, QWidget* parent)
        : QMainWindow{parent},
          ui_{new Ui::LiveWindow},
          gameLogRuntime_{gameLogRuntime}
    {
        ui_->setupUi(this);

        connect(gameLogRuntime_.getSessionService(),
                &application::services::SessionService::sessionStarted,
                this,
                &LiveWindow::onSessionStarted);
        connect(gameLogRuntime_.getSessionService(),
                &application::services::SessionService::sessionStopped,
                this,
                &LiveWindow::onSessionFinished);

        clockTimer_ = new QTimer{this};
        clockTimer_->setInterval(1000);
        connect(clockTimer_, &QTimer::timeout, this, &LiveWindow::updateTimerText);
        currentTime_ = QTime{0, 0};
    }

    LiveWindow::~LiveWindow() { delete ui_; }

    void LiveWindow::onSessionStarted(const core::domain::Game& game)
    {
        QGridLayout* layout = ui_->mainGridLayout;

        // Remove and delete the old game card widget.
        layout->removeWidget(ui_->cardWidget);
        delete ui_->cardWidget;

        // Create and add the new widget.
        ui_->cardWidget = new GameCard{ui_->centralwidget, game, gameLogRuntime_.getArtworkService()};
        layout->addWidget(ui_->cardWidget, 0, 0); // Insert at position 0 (first).

        // Requesting artwork is the window's decision, not the card's. The card
        // redraws itself when artworkAvailable arrives for this game.
        if(auto* artworkService = gameLogRuntime_.getArtworkService(); artworkService != nullptr)
        {
            static_cast<void>(artworkService->getGameArtwork(game));
        }

        // Set up the timer from the persisted active-session start.
        if(const auto session = gameLogRuntime_.getSessionService()->findActiveSession())
        {
            // QTime wraps after 24 hours and addMSecs() takes an int, so clamp
            // the elapsed time to just under a full day rather than narrowing.
            constexpr qint64 millisecondsPerDay = 24LL * 60LL * 60LL * 1000LL;
            const qint64 elapsed = std::max<qint64>(0,
                                                    session->startTimestamp.
                                                             msecsTo(QDateTime::currentDateTimeUtc()));
            currentTime_ = QTime{0, 0}.addMSecs(static_cast<int>(std::min(elapsed, millisecondsPerDay - 1)));
        }
        else { currentTime_ = QTime{0, 0}; }
        clockTimer_->start();

        // Enable note taking.
        ui_->textEditor->setAbleToEdit(true);
    }

    void LiveWindow::onSessionFinished(core::domain::Session completedSession)
    {
        clockTimer_->stop();

        QGridLayout* layout = ui_->mainGridLayout;

        // Remove and delete the old game card widget.
        layout->removeWidget(ui_->cardWidget);
        delete ui_->cardWidget;

        // Create and add the empty widget.
        ui_->cardWidget = new GameCard{ui_->centralwidget, {}, gameLogRuntime_.getArtworkService()};
        layout->addWidget(ui_->cardWidget, 0, 0); // Insert at position 0 (first).

        ui_->timeLabel->setText(QStringLiteral("Session Completed\n 00:00:00"));
        ui_->textEditor->setAbleToEdit(false);

        completedSession.notes = ui_->textEditor->getMarkdown();
        if(!gameLogRuntime_.getSessionService()->updateSession(completedSession))
        {
            qCWarning(gamelogGuiLog) << "Failed to update session notes for session id:" << completedSession.id;
        }
    }

    void LiveWindow::updateTimerText()
    {
        ui_->timeLabel->setText(currentTime_.toString());
        currentTime_ = currentTime_.addSecs(1);
    }
} // namespace gamelog::gui
