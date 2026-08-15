//
// Created by kj on 8/6/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_GameCard.h" resolved

#include "GameCard.h"

#include <resources/AppPaths.h>

#include "ui_gamecard.h"

using gamelog::core::domain::Game;

GameCard::GameCard(QWidget* parent, const Game& game, gamelog::application::services::GameArtworkService* artworkService): QWidget(parent), ui(new Ui::GameCard)
{
    ui->setupUi(this);

    artworkService_ = artworkService;

    ui->gameArtLabel->setAlignment(Qt::AlignCenter);
    ui->gameArtLabel->setScaledContents(false);
    artworkService_->getGameArtwork(const_cast<Game&>(game));

    QPixmap imageMap = QPixmap(gamelog::core::AppPaths::artworkDirectory() + "/" + QString::number(game.id) + "/cover.jpg");
    imageMap = imageMap.scaled(ui->gameArtLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    ui->gameArtLabel->setPixmap(imageMap);
    ui->gameTitleLabel->setText(game.title);
}

GameCard::~GameCard()
{
    delete ui;
}


QSize GameCard::sizeHint() const
{
    return {135, 240};
}

QSize GameCard::minimumSizeHint() const
{
    return {90, 160};
}

bool GameCard::hasHeightForWidth() const
{
    return true;
}

int GameCard::heightForWidth(int width) const
{
    return width * 9 / 16;
}
