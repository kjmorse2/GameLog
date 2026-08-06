//
// Created by kj on 8/3/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_LIbraryView.h" resolved

#include "LibraryView.h"

#include <QPushButton>
#include <QObject>

#include "ui_libraryview.h"

using gamelog::application::services::GameService;

LibraryView::LibraryView(QWidget *parent, GameService *service) :
    QWidget(parent), ui(new Ui::LibraryView), service(service)
{
    ui->setupUi(this);
    connect(ui->refreshButton, &QPushButton::clicked, service, &GameService::listGames);
    connect(service, &GameService::gamesFound, this, &LibraryView::displayAllGames);
    service->listGames();
}

LibraryView::~LibraryView()
{
    delete ui;
}

void LibraryView::displayAllGames(const std::vector<gamelog::core::domain::Game> &games)
{
    ui->gameListWidget->clear();
    gameList_ = games;
    for (const auto &game : gameList_)
    {
        QString label = game.title;
        if (!game.trackingEnabled)
        {
            label += QStringLiteral(" (tracking disabled)");
        }
        ui->gameListWidget->addItem(label);
    }
}