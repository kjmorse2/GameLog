//
// Created by kj on 8/3/26.
//


#ifndef GAMELOG_LIBRARYVIEW_H
#define GAMELOG_LIBRARYVIEW_H

#include <QListWidget>
#include <QWidget>

#include "application/services/GameService.h"
#include "domain/Game.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class LibraryView;
}
QT_END_NAMESPACE

class LibraryView : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryView(QWidget *parent = nullptr, gamelog::application::services::GameService *service = nullptr);
    ~LibraryView() override;

    void displayAllGames(const std::vector<gamelog::core::domain::Game>&);

private:
    Ui::LibraryView *ui{};
    std::vector<gamelog::core::domain::Game> gameList_;
    gamelog::application::services::GameService *service;

};


#endif // GAMELOG_LIBRARYVIEW_H
