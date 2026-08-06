//
// Created by kj on 8/5/26.
//

#ifndef GAMELOG_REFRESHBUTTON_H
#define GAMELOG_REFRESHBUTTON_H
#include <QPushButton>


class RefreshButton : public QPushButton
{
    Q_OBJECT

public:
    explicit RefreshButton(QWidget *parent = nullptr);

signals:

    void refreshRequested();
};


#endif // GAMELOG_REFRESHBUTTON_H
