#include "MainWindow.h"

#include <optional>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

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
        auto* libraryViewWidget = new LibraryView{ui->libraryTab, &runtime};
        ui->libraryTabLayout->addWidget(libraryViewWidget);

        ui->calanderTabLayout->addWidget(new CalendarView{ui->calanderTab, runtime.getSessionService()});

        statusActiveLabel_ = new QLabel{ui->statusBar};
        statusTitleLabel_ = new QLabel{ui->statusBar};
        statusTimeLabel_ = new QLabel{ui->statusBar};

        ui->statusBar->addPermanentWidget(statusActiveLabel_);
        ui->statusBar->addPermanentWidget(statusTitleLabel_);
        ui->statusBar->addPermanentWidget(statusTimeLabel_);

        const auto sessionService = runtime_.getSessionService();

        connect(sessionService, &SessionService::sessionStarted, this, &MainWindow::onSessionStarted);
        connect(runtime_.getSessionService(), &SessionService::sessionStopped, this, &MainWindow::onSessionEnded);
        connect(ui->actionAdd_SteamAPI_Key, &QAction::triggered, this, &MainWindow::onAddSteamApiKey);
        connect(ui->actionAdd_Steam_Player_ID, &QAction::triggered, this, &MainWindow::onAddSteamPlayerId);
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
        QDialog dialog{this};
        dialog.setWindowTitle(title);
        dialog.setModal(true);

        auto* layout = new QVBoxLayout{&dialog};

        auto* explanationLabel = new QLabel{explanation, &dialog};
        explanationLabel->setWordWrap(true);

        auto* secretEdit = new QLineEdit{&dialog};
        secretEdit->setEchoMode(QLineEdit::Password);
        secretEdit->setPlaceholderText(placeholder);

        auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
        buttons->button(QDialogButtonBox::Ok)->setText(tr("Submit"));

        layout->addWidget(explanationLabel);
        layout->addWidget(secretEdit);
        layout->addWidget(buttons);

        connect(buttons,
                &QDialogButtonBox::accepted,
                &dialog,
                [&dialog, secretEdit]
                {
                    // A blank entry keeps the dialog open rather than storing nothing.
                    if(secretEdit->text().trimmed().isEmpty()) { return; }
                    dialog.accept();
                });

        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if(dialog.exec() != QDialog::Accepted) { return std::nullopt; }

        return secretEdit->text().trimmed();
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
