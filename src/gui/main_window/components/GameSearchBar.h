//
// Created by kj on 8/21/26.
//

#ifndef GAMELOG_GAMESEARCHBAR_H
#define GAMELOG_GAMESEARCHBAR_H

#include <QWidget>


namespace gamelog::core::domain {
    struct Game;
}

namespace gamelog::application::services {
    class GameService;
}

QT_BEGIN_NAMESPACE

namespace Ui
{
    class GameSearchBar;
}

QT_END_NAMESPACE

class GameSearchBar : public QWidget
{
    Q_OBJECT

public:
    explicit GameSearchBar(QWidget* parent = nullptr);
    ~GameSearchBar() override;
    void initialize(gamelog::application::services::GameService* service);

Q_SIGNALS:
    void gameChosen(const gamelog::core::domain::Game& game);

private:
    bool initialized{false};
    Ui::GameSearchBar* ui;
    std::vector<gamelog::core::domain::Game> games;
    QHash<QString, gamelog::core::domain::Game> gameTitles;
    gamelog::application::services::GameService* gameService{};

private Q_SLOTS:
    void onGameChosen();
};


#endif //GAMELOG_GAMESEARCHBAR_H
