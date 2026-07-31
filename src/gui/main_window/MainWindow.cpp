//
// Created by kj on 7/31/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_main_window.h" resolved

#include "MainWindow.h"
#include "ui_MainWindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}
