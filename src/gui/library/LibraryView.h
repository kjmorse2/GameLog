//
// Created by kj on 8/3/26.
//

#ifndef GAMELOG_LIBRARYVIEW_H
#define GAMELOG_LIBRARYVIEW_H

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui {
    class LibraryView;
}
QT_END_NAMESPACE

class LibraryView : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryView(QWidget *parent = nullptr);
    ~LibraryView() override;

private:
    Ui::LibraryView *ui;
};


#endif // GAMELOG_LIBRARYVIEW_H
