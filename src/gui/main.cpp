#include "main_window/MainWindow.h"

#include <QApplication>

/*
    @Date: 03/30/2026
    @Description: Constructs the program and connects the model and view together.
    @Verified By: Brian Keller
*/

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w{};
    w.show();
    return QCoreApplication::exec();
}