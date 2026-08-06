//
// Created by kj on 8/5/26.
//

#include "RefreshButton.h"
RefreshButton::RefreshButton(QWidget *parent)
    : QPushButton(parent)
{
    connect(this, &QPushButton::clicked, this, &RefreshButton::refreshRequested);
}