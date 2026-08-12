//
// Created by kj on 8/12/26.
//

#ifndef GAMELOG_LIVEWINDOW_H
#define GAMELOG_LIVEWINDOW_H

#include <QMainWindow>
#include <QTime>

class QTimer;

#include "application/GameLogRuntime.h"

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
        slots:

        void onSessionStarted(Game game);

        void updateTimerText();

    private:
        Ui::LiveWindow* ui{};
        application::GameLogRuntime& gameLogRuntime;
        QTimer* clockTimer;
        QTime currentTime;
    };
} // namespace gamelog::gui

#endif // GAMELOG_LIVEWINDOW_H
