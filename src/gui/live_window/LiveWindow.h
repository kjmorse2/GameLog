#pragma once

#include <QMainWindow>
#include <QTime>

#include "application/GameLogRuntime.h"
#include "core/domain/Game.h"
#include "core/domain/Session.h"

class QTimer;

QT_BEGIN_NAMESPACE

namespace Ui
{
    class LiveWindow;
}

QT_END_NAMESPACE

namespace gamelog::gui
{
    /**
     * Compact window showing the session that is currently being tracked.
     *
     * Purely reactive: it starts and stops nothing itself, but follows
     * SessionService::sessionStarted/sessionStopped, swapping in the detected
     * game's card and running a one-second display timer. Its one write is to
     * persist the note text back to the session when that session ends.
     */
    class LiveWindow final : public QMainWindow
    {
        Q_OBJECT

    public:
        /**
         * Constructs a LiveWindow bound to the running runtime.
         *
         * @pre The runtime must have been started successfully. The windows
         * dereference GameLogRuntime's service accessors without null checks,
         * and those return nullptr when database initialization failed.
         * main.cpp enforces this by exiting when start() returns false.
         * @param gameLogRuntime The started runtime supplying session state.
         * @param parent The parent widget.
         */
        explicit LiveWindow(application::GameLogRuntime& gameLogRuntime, QWidget* parent = nullptr);

        ~LiveWindow() override;

    private Q_SLOTS:
        /**
         * Connected to SessionService::sessionStarted. Updates the UI to reflect the new session.
         * @param game The Game struct that was started.
         */
        void onSessionStarted(const core::domain::Game& game);

        /**
         * Connected to SessionService::sessionStopped. The Session is received by
         * value so this slot may add notes to its own copy and explicitly persist them.
         * @param completedSession The Session struct that was completed or interrupted.
         */
        void onSessionFinished(core::domain::Session completedSession);

        /**
         * Updates the visible elapsed-session timer.
         */
        void updateTimerText();

    private:
        Ui::LiveWindow* ui_{};
        application::GameLogRuntime& gameLogRuntime_;
        QTimer* clockTimer_{};
        QTime currentTime_;
    };
} // namespace gamelog::gui
