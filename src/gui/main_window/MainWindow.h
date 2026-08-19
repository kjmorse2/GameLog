#pragma once

#include <optional>

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
         *
         * @pre The runtime must have been started successfully. The windows
         * dereference GameLogRuntime's service accessors without null checks,
         * and those return nullptr when database initialization failed.
         * main.cpp enforces this by exiting when start() returns false.
         * @param runtime The started runtime supplying game and session state.
         * @param parent The parent widget.
         */
        explicit MainWindow(application::GameLogRuntime& runtime, QWidget* parent = nullptr);

        ~MainWindow() override;

    private:
        /**
         * Shows a modal password-style prompt for one credential.
         *
         * Submitting is refused while the field is blank after trimming, so a
         * returned value is always non-empty.
         * @return The entered secret, or std::nullopt if the dialog was cancelled.
         */
        [[nodiscard]] std::optional<QString> promptForSecret(const QString& title,
                                                             const QString& explanation,
                                                             const QString& placeholder);

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
