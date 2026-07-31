//
// Created by kj on 7/31/26.
//

#ifndef GAMELOG_MAIN_WINDOW_H
#define GAMELOG_MAIN_WINDOW_H


#include <QMainWindow>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
};

#endif // GAMELOG_MAIN_WINDOW_H
