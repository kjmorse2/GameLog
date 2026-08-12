//
// Created by kj on 8/3/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_LIbraryView.h" resolved

#include "LibraryView.h"

#include <QObject>
#include "gui/game_card/GameCard.h"

#include "ui_libraryview.h"

using gamelog::application::services::GameService;

LibraryView::LibraryView(QWidget* parent, GameService* service) : QWidget(parent), ui(new Ui::LibraryView), service(service)
{
    ui->setupUi(this);
    ui->gridLayout->setContentsMargins(0, 0, 0, 0);
    ui->gridLayout->setSpacing(12);
    ui->gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    connect(ui->refreshButton, &QPushButton::clicked, service, &GameService::listGames);
    connect(service, &GameService::gamesFound, this, &LibraryView::displayAllGames);
    service->listGames();
}

LibraryView::~LibraryView()
{
    delete ui;
}

void LibraryView::displayAllGames(const std::vector<gamelog::core::domain::Game>& games)
{
    while (QLayoutItem* item = ui->gameGridLayout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            widget->setParent(nullptr);
            delete widget;
        }
        delete item;
    }

    for (int i = 0; i < games.size(); ++i)
    {
        constexpr int columns = 4;
        const int row = i / columns;
        const int col = i % columns;

        auto* gameCard = new GameCard(ui->gameGridContainer, games[i]);
        ui->gameGridLayout->addWidget(gameCard, row, col);
    }
}