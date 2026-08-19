//
// Created by kj on 8/12/26.
//

#ifndef GAMELOG_LIVEWINDOW_H
#define GAMELOG_LIVEWINDOW_H

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
    class LiveWindow final : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit LiveWindow(application::GameLogRuntime& gameLogRuntime, QWidget* parent = nullptr);

        ~LiveWindow() override;

    private
        slots  :
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
        Ui::LiveWindow* ui{};
        application::GameLogRuntime& gameLogRuntime;
        QTimer* clockTimer{};
        QTime currentTime;
    };
} // namespace gamelog::gui

#endif // GAMELOG_LIVEWINDOW_H
