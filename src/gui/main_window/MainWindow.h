#pragma once

#include <QMainWindow>
#include "domain/Game.h"

class QLabel;

namespace gamelog::application
{
    class GameLogRuntime;
}

QT_BEGIN_NAMESPACE

namespace Ui
{
    class MainWindow;
}

QT_END_NAMESPACE

namespace gamelog::gui
{
    /** Minimal GUI shell that queries the in-process runtime directly. */
    class MainWindow final : public QMainWindow
    {
        Q_OBJECT

    public:
        /**
     * @brief Constructs a new MainWindow.
     * @param runtime
     * @param parent
     */
        explicit MainWindow(application::GameLogRuntime& runtime, QWidget* parent = nullptr);

        ~MainWindow() override;

    Q_SIGNALS:
        void steamApiKeyEntered(QString key);

        void steamPlayerIdEntered(QString playerId);

    private Q_SLOTS:
        void onSessionStarted(const core::domain::Game& game);

        void onSessionEnded();

        void onAddSteamApiKey();

        void onAddSteamPlayerId();

    private:
        Ui::MainWindow* ui;
        application::GameLogRuntime& runtime_;
        QLabel* statusActiveLabel_;
        QLabel* statusTitleLabel_;
        QLabel* statusTimeLabel_;
    };
} // namespace gamelog::gui
