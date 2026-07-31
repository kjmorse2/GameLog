//
// Created by kj on 7/31/26.
//

#ifndef GAMELOG_GAMECARD_H
#define GAMELOG_GAMECARD_H

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui {
    class GameCard;
}
QT_END_NAMESPACE

class GameCard : public QWidget
{
    Q_OBJECT

public:
    explicit GameCard(QWidget *parent = nullptr);
    ~GameCard() override;

private:
    Ui::GameCard *ui;
};


#endif // GAMELOG_GAMECARD_H
