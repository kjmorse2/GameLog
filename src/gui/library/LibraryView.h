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

namespace Ui
{
    class LibraryView;
}

QT_END_NAMESPACE

class LibraryView : public QWidget
{
    Q_OBJECT

public:
    /**
     * Constructs a LibraryView.
     * @param parent The parent widget.
     * @param service The Game service to querey Games for.
     */
    explicit LibraryView(QWidget* parent = nullptr, gamelog::application::services::GameService* service = nullptr);

    ~LibraryView() override;

    /**
     * Displays all games in the library.
     * @param games The list of games to display.
     */
    void displayAllGames(const std::vector<Game>& games);

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
    gamelog::application::services::GameService* service;
};


#endif // GAMELOG_LIBRARYVIEW_H
