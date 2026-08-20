#include "MainWindow.h"

#include <optional>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMenu>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "SecretDialog.h"
#include "gui/calendar/CalendarView.h"
#include "gui/library/LibraryView.h"
#include "application/GameLogRuntime.h"
#include "application/services/local/SessionService.h"

#include "ui_mainwindow.h"

using gamelog::application::services::SessionService;

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
    MainWindow::MainWindow(application::GameLogRuntime& runtime, QWidget* parent) : QMainWindow{parent},
        ui(new Ui::MainWindow),
        runtime_{runtime}
    {
        initializeGUIResources();
        ui->setupUi(this);

        // Setup the "Add" button with a menu for adding games or sessions
        auto *addMenu = new QMenu(ui->addButton);
        QAction *addGameAction = addMenu->addAction("Add Game");
        QAction *addSessionAction = addMenu->addAction("Add Session");
        ui->addButton->setMenu(addMenu);
        // TODO: Connect to functionality.

        // Setup the "Config" button with a menu for configuration options
        auto *configMenu = new QMenu(ui->configButton);

        const QAction *addSteamApiKeyAction = configMenu->addAction("Steam API Key");
        connect(addSteamApiKeyAction, &QAction::triggered, this, &MainWindow::onAddSteamApiKey);

        const QAction *addSteamPlayerIdAction = configMenu->addAction("Add Steam Player ID");
        connect(addSteamPlayerIdAction, &QAction::triggered, this, &MainWindow::onAddSteamPlayerId);

        QAction *openSettingsAction = configMenu->addAction("Settings");
        QAction *fullShutdownAction = configMenu->addAction("Full Shutdown");
        ui->configButton->setMenu(configMenu);
        // TODO: Connect to functionality.

        // Setup the library and calendar views
        auto* libraryViewWidget = new LibraryView{ui->libraryTab, &runtime};
        ui->libraryTabLayout->addWidget(libraryViewWidget);
        ui->calanderTabLayout->addWidget(new CalendarView{ui->calanderTab, runtime.getSessionService()});

        // Setup the status bar labels
        statusActiveLabel_ = new QLabel{ui->statusBar};
        statusTitleLabel_ = new QLabel{ui->statusBar};
        statusTimeLabel_ = new QLabel{ui->statusBar};

        ui->statusBar->addPermanentWidget(statusActiveLabel_);
        ui->statusBar->addPermanentWidget(statusTitleLabel_);
        ui->statusBar->addPermanentWidget(statusTimeLabel_);

        const auto sessionService = runtime_.getSessionService();

        connect(sessionService, &SessionService::sessionStarted, this, &MainWindow::onSessionStarted); connect(runtime_.getSessionService(), &SessionService::sessionStopped, this, &MainWindow::onSessionEnded); // connect(ui->actionAdd_Steam_Player_ID, &QAction::triggered, this, &MainWindow::onAddSteamApiKey);
    }

    MainWindow::~MainWindow() { delete ui; }

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

    std::optional<QString> MainWindow::promptForSecret(const QString& title,
                                                        const QString& explanation,
                                                        const QString& placeholder)
    {
        SecretDialog dialog{title, explanation, placeholder, this};

        if(dialog.exec() != QDialog::Accepted)
        {
            return std::nullopt;
        }
        QString secret = dialog.secret().trimmed();
        dialog.close();

        return secret;
    }

    void MainWindow::onAddSteamApiKey()
    {
        const std::optional<QString> key = promptForSecret(tr("Add Steam Web API Key"),
                                                           tr("GameLog uses the Steam Web API to import your Steam library. "
                                                              "Enter your Steam Web API key below. "
                                                              "You can obtain a key from Steam's developer page."),
                                                           tr("Steam Web API key"));

        if(!key) { return; }

        runtime_.getCredentialService()->
                 setSecret(QString::fromLatin1(application::services::CredentialService::kSteamApiKey), *key);
    }

    void MainWindow::onAddSteamPlayerId()
    {
        const std::optional<QString> playerId = promptForSecret(tr("Add Steam Player ID"),
                                                                tr("GameLog uses the Steam Web API to import your Steam library. "
                                                                   "Enter your 64-bit Steam player ID below. "
                                                                   "You can find it on your Steam profile page."),
                                                                tr("Steam player ID"));

        if(!playerId) { return; }

        runtime_.getCredentialService()->
                 setSecret(QString::fromLatin1(application::services::CredentialService::kSteamPlayerIdKey), *playerId);
    }
} // namespace gamelog::gui
