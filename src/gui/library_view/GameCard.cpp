//
// Created by kj on 7/31/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_GameCard.h" resolved

#include "GameCard.h"
#include "ui_GameCard.h"


GameCard::GameCard(QWidget *parent) : QWidget(parent), ui(new Ui::GameCard)
{
    ui->setupUi(this);
}

GameCard::~GameCard()
{
    delete ui;
}
