//
// Created by kj on 8/7/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_CalendarView.h" resolved

#include "CalendarView.h"
#include "ui_calendarview.h"


CalendarView::CalendarView(QWidget *parent, GameService *gameService, SessionService *sessionService ) :
    QWidget(parent), ui(new Ui::CalendarView)
{
    ui->setupUi(this);
    gameService_ = gameService;
    sessionService_ = sessionService;
    calendar_ = ui->calendarWidget;
    connect(calendar_, &QCalendarWidget::currentPageChanged, this, &CalendarView::onPageChanged);
}

CalendarView::~CalendarView()
{
    delete ui;
}

void CalendarView::onPageChanged(int year, int month);
{
    return;
}
