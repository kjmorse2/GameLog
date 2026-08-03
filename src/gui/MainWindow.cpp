#include "gui/MainWindow.h"

#include "application/GameLogRuntime.h"
#include "domain/Game.h"
#include "domain/Session.h"

#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace gamelog::gui {

MainWindow::MainWindow(
    application::GameLogRuntime &runtime,
    QWidget *parent)
    : QMainWindow{parent}, runtime_{runtime}
{
    setWindowTitle(QStringLiteral("GameLog"));
    resize(720, 480);

    auto *centralWidget = new QWidget{this};
    auto *layout = new QVBoxLayout{centralWidget};

    auto *titleLabel = new QLabel{QStringLiteral("GameLog library"), centralWidget};
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    statusLabel_ = new QLabel{centralWidget};
    gameList_ = new QListWidget{centralWidget};
    auto *reloadButton = new QPushButton{
        QStringLiteral("Reload library"), centralWidget};

    layout->addWidget(titleLabel);
    layout->addWidget(statusLabel_);
    layout->addWidget(gameList_, 1);
    layout->addWidget(reloadButton);

    setCentralWidget(centralWidget);

    connect(
        reloadButton,
        &QPushButton::clicked,
        this,
        [this] {
            static_cast<void>(runtime_.reloadTrackedGames());
            refreshLibrary();
        });

    refreshLibrary();
}

void MainWindow::refreshLibrary()
{
    gameList_->clear();

    const auto games = runtime_.listGames();

    for (const core::domain::Game &game : games)
    {
        QString label = game.title;

        if (!game.trackingEnabled)
        {
            label += QStringLiteral(" (tracking disabled)");
        }

        gameList_->addItem(label);
    }

    const auto session = runtime_.activeSession();

    if (session)
    {
        statusLabel_->setText(
            QStringLiteral("Active session #%1 for game ID %2 | %3 games")
                .arg(session->id)
                .arg(session->gameId)
                .arg(static_cast<qulonglong>(games.size())));
    }
    else
    {
        statusLabel_->setText(
            QStringLiteral("No active session | %1 games")
                .arg(static_cast<qulonglong>(games.size())));
    }
}

} // namespace gamelog::gui
