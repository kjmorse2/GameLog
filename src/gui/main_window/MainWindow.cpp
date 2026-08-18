#include "MainWindow.h"

#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QWidget>
#include <QDialogButtonBox>
#include <QDialog>
#include <QPushButton>
#include <qloggingcategory.h>
#include <logging/LoggingCategories.h>

#include "gui/calendar/CalendarView.h"
#include "gui/library/LibraryView.h"
#include "application/GameLogRuntime.h"
#include "../../application/services/local/SessionService.h"

#include "ui_mainwindow.h"

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

        ui->calanderTabLayout->addWidget(new CalendarView{
                                             ui->calanderTab, runtime.getGameService(), runtime.getSessionService()
                                         });

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
        connect(this,
                &MainWindow::steamAPIKeyEntered,
                runtime_.getCredentialService(),
                [credentialService = runtime_.getCredentialService()](const QString& key)
                {
                    credentialService->
                        setSecret(QString::fromLatin1(application::services::CredentialService::kSteamApiKey), key);
                });
        connect(ui->actionAdd_Steam_Player_ID, &QAction::triggered, this, &MainWindow::onAddSteamPlayerId);
        connect(this,
                &MainWindow::steamPlayerIdEntered,
                runtime_.getCredentialService(),
                [credentialService = runtime_.getCredentialService()](const QString& key)
                {
                    credentialService->
                        setSecret(QString::fromLatin1(application::services::CredentialService::kSteamPlayerIdKey),
                                  key);
                });
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

    void MainWindow::onAddSteamApiKey()
    {
        QDialog dialog{this};
        dialog.setWindowTitle(tr("Add Steam Web API Key"));
        dialog.setModal(true);

        auto* layout = new QVBoxLayout{&dialog};

        auto* explanation = new QLabel{
            tr("GameLog uses the Steam Web API to import your Steam library. " "Enter your Steam Web API key below. "
               "You can obtain a key from Steam's developer page."),
            &dialog
        };
        explanation->setWordWrap(true);

        auto* keyEdit = new QLineEdit{&dialog};
        keyEdit->setEchoMode(QLineEdit::Password);
        keyEdit->setPlaceholderText(tr("Steam Web API key"));

        auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};

        buttons->button(QDialogButtonBox::Ok)->setText(tr("Submit"));

        layout->addWidget(explanation);
        layout->addWidget(keyEdit);
        layout->addWidget(buttons);

        connect(buttons,
                &QDialogButtonBox::accepted,
                &dialog,
                [&dialog, keyEdit, this]
                {
                    const QString key = keyEdit->text().trimmed();

                    if(key.isEmpty()) { return; }

                    emit steamAPIKeyEntered(key);
                    dialog.accept();
                });

        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        dialog.exec();
    }

    void MainWindow::onAddSteamPlayerId()
    {
        QDialog dialog{this};
        dialog.setWindowTitle(tr("Add Steam Player Id"));
        dialog.setModal(true);

        auto* layout = new QVBoxLayout{&dialog};

        auto* explanation = new QLabel{
            tr("GameLog uses the Steam Web API to import your Steam library. " "Enter your Steam player API key below. "
               "You can obtain a key from Steam's developer page."),
            &dialog
        };
        explanation->setWordWrap(true);

        auto* keyEdit = new QLineEdit{&dialog};
        keyEdit->setEchoMode(QLineEdit::Password);
        keyEdit->setPlaceholderText(tr("Steam player ID"));

        auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};

        buttons->button(QDialogButtonBox::Ok)->setText(tr("Submit"));

        layout->addWidget(explanation);
        layout->addWidget(keyEdit);
        layout->addWidget(buttons);

        connect(buttons,
                &QDialogButtonBox::accepted,
                &dialog,
                [&dialog, keyEdit, this]
                {
                    const QString key = keyEdit->text().trimmed();

                    if(key.isEmpty()) { return; }

                    emit steamPlayerIdEntered(key);
                    dialog.accept();
                });

        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        dialog.exec();
    }
} // namespace gamelog::gui
