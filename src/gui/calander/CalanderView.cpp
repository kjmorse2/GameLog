//
// Created by kj on 8/6/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_CalanderView.h" resolved

#include "CalanderView.h"
#include "ui_calanderview.h"


CalanderView::CalanderView(QWidget *parent) :
    QWidget(parent), ui(new Ui::CalanderView)
{
    ui->setupUi(this);
}

CalanderView::~CalanderView()
{
    delete ui;
}
