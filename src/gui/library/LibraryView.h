//
// Created by kj on 8/3/26.
//


#ifndef GAMELOG_LIBRARYVIEW_H
#define GAMELOG_LIBRARYVIEW_H

#include <QListWidget>
#include <QWidget>

#include "application/services/GameService.h"
#include "domain/Game.h"

namespace gamelog::application
{
    class GameLogRuntime;
}

QT_BEGIN_NAMESPACE

namespace Ui
{
    class LibraryView;
}

QT_END_NAMESPACE

class LibraryView:public QWidget
{
    Q_OBJECT

public:
    /**
     * Constructs a LibraryView.
     * @param parent The parent widget.
     * @param service The Gamelog service to querey Games for.
     */
    explicit LibraryView(QWidget* parent = nullptr, gamelog::application::GameLogRuntime* service = nullptr);

    ~LibraryView() override;

public
    slots:
    /**
     * Displays all games in the library.
     */


    void displayAllGames();

private:
    /**
     * @brief UI pointer from QT
     */
    Ui::LibraryView* ui{};

    /**
     * @brief The list of games on display in the library.
     */
    std::vector<Game> gameList_;

    /**
     * @brief The Game service to query Games for.
     */
    gamelog::application::GameLogRuntime* runtime_;
};


#endif // GAMELOG_LIBRARYVIEW_H
