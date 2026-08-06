//
// Created by kj on 8/6/26.
//

#ifndef GAMELOG_CALANDERVIEW_H
#define GAMELOG_CALANDERVIEW_H

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui {
    class CalanderView;
}
QT_END_NAMESPACE

class CalanderView : public QWidget
{
    Q_OBJECT

public:
    explicit CalanderView(QWidget *parent = nullptr);
    ~CalanderView() override;

private:
    Ui::CalanderView *ui{};
};


#endif // GAMELOG_CALANDERVIEW_H
