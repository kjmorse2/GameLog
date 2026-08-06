//
// Created by kj on 8/6/26.
//

#ifndef GAMELOG_GAMECARD_H
#define GAMELOG_GAMECARD_H

#include <QWidget>
#include "core/domain/Game.h"


QT_BEGIN_NAMESPACE
namespace Ui {
    class GameCard;
}
QT_END_NAMESPACE

class GameCard : public QWidget
{
    Q_OBJECT

public:
    explicit GameCard(QWidget *parent = nullptr, gamelog::core::domain::Game game = {});
    ~GameCard() override;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

private:
    Ui::GameCard *ui;
};


#endif // GAMELOG_GAMECARD_H
