//
// Created by kj on 8/7/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_CalendarView.h" resolved

#include <QTextCharFormat>
#include "CalendarView.h"
#include "ui_calendarview.h"


CalendarView::CalendarView(QWidget* parent, GameService* gameService, SessionService* sessionService): QWidget(parent),
                                                                                                       ui(new Ui::CalendarView)
{
    ui->setupUi(this);
    gameService_ = gameService;
    sessionService_ = sessionService;
    calendar_ = ui->calendarWidget;
    connect(calendar_, &QCalendarWidget::currentPageChanged, this, &CalendarView::onPageChanged);
    emit
    calendar_->currentPageChanged(calendar_->yearShown(), calendar_->monthShown());
}

CalendarView::~CalendarView()
{
    delete ui;
}

void CalendarView::onPageChanged(int year, int month)
{
    QDateTime startDate = QDateTime{QDate(year, month, 1), QTime{0, 0}};
    QDateTime endDate = QDateTime{QDate(year, (month + 1) % 12, 1), QTime{0, 0}};
    vector<Session> foundSessions = sessionService_->getSessionsInTimeRange(startDate, endDate);
    for(const auto& session : foundSessions)
    {
        QDate date = session.startTimestamp.date();

        QTextCharFormat format;
        format.setBackground(QBrush{Qt::red});
        format.setForeground(QBrush{Qt::white});

        ui->calendarWidget->setDateTextFormat(date, format);
    }
}
