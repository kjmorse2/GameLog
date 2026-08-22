//
// Created by kj on 8/21/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_GameSearchBar.h" resolved

#include "GameSearchBar.h"

#include <QMenu>
#include <QCompleter>
#include <qstringlistmodel.h>

#include "ui_gamesearchbar.h"
#include "application/services/local/GameService.h"
#include "application/services/local/SessionService.h"


GameSearchBar::GameSearchBar(QWidget* parent) : QWidget(parent), ui(new Ui::GameSearchBar)
{
    ui->setupUi(this);
}

void GameSearchBar::initialize(gamelog::application::services::GameService* service)
{
    gameService = service;
    games = gameService->listGames();
    for(const auto& game : games)
    {
        gameTitles.insert(game.title, game);
    }

    auto* model = new QStringListModel{this};
    model->setStringList(gameTitles.keys());
    auto* completer = new QCompleter{model, this};
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);   // substring — matches your title.contains(text)
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setMaxVisibleItems(10);             // replaces the found < 10 cap
    ui->searchBar->setCompleter(completer);
    connect(completer, qOverload<const QString&>(&QCompleter::activated), this, &GameSearchBar::onGameChosen);
    connect(ui->clearButton, &QPushButton::clicked, this, &GameSearchBar::onSelectionCleared);

    initialized = true;
}

GameSearchBar::~GameSearchBar() { delete ui; }

void GameSearchBar::onGameChosen()
{
    if(!initialized) { return; }

    const QString chosenTitle = ui->searchBar->text();
    if(!gameTitles.contains(chosenTitle)) { return; }
    const gamelog::core::domain::Game game = gameTitles.value(chosenTitle);

    emit gameSelected(game);
}

void GameSearchBar::onSelectionCleared()
{
    if(!initialized) { return; }
    ui->searchBar->clear();
    emit gameSelectionCleared();
}