//
// Created by kj on 8/3/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_LIbraryView.h" resolved

#include "LibraryView.h"

#include <QObject>
#include <application/GameLogRuntime.h>
#include <application/services/GameService.h>
#include <application/services/GameArtworkService.h>

#include "gui/game_card/GameCard.h"

#include "ui_libraryview.h"

using gamelog::application::services::GameService;

LibraryView::LibraryView(QWidget* parent, gamelog::application::GameLogRuntime* runtime): QWidget(parent),
                                                                                          ui(new Ui::LibraryView),
                                                                                          runtime_(runtime)
{
    ui->setupUi(this);
    ui->gridLayout->setContentsMargins(0, 0, 0, 0);
    ui->gridLayout->setSpacing(12);
    ui->gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    connect(ui->refreshButton, &QPushButton::clicked, this, &LibraryView::displayAllGames);
    displayAllGames();
}

LibraryView::~LibraryView()
{
    delete ui;
}

void LibraryView::displayAllGames()
{
    const auto games = runtime_->getGameService()->listGames();
    while(const QLayoutItem* item = ui->gameGridLayout->takeAt(0))
    {
        if(QWidget* widget = item->widget())
        {
            widget->setParent(nullptr);
            delete widget;
        }
        delete item;
    }

    for(auto i = 0; i < games.size(); ++i)
    {
        constexpr auto columns = 4;
        const int row = i / columns;
        const int col = i % columns;

        auto* gameCard = new GameCard(ui->gameGridContainer, games[i], runtime_->getArtworkService());
        ui->gameGridLayout->addWidget(gameCard, row, col);
    }
}