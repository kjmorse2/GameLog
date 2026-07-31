//
// Created by kj on 7/31/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_LibraryView.h" resolved

#include "LibraryView.h"
#include "ui_LibraryView.h"


LibraryView::LibraryView(QWidget *parent) :
    QWidget(parent), ui(new Ui::LibraryView)
{
    ui->setupUi(this);
}

LibraryView::~LibraryView()
{
    delete ui;
}
