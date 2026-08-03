//
// Created by kj on 8/3/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_LIbraryView.h" resolved

#include "LibraryView.h"
#include "ui_LIbraryView.h"


LIbraryView::LIbraryView(QWidget *parent) :
    QWidget(parent), ui(new Ui::LIbraryView)
{
    ui->setupUi(this);
}

LIbraryView::~LIbraryView()
{
    delete ui;
}
