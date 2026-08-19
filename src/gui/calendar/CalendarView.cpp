//
// Created by kj on 8/7/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_CalendarView.h" resolved

#include "CalendarView.h"
#include <QTextCharFormat>
#include "ui_calendarview.h"


CalendarView::CalendarView(QWidget* parent, GameService* gameService, SessionService* sessionService) : QWidget(parent),
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

CalendarView::~CalendarView() { delete ui; }

void CalendarView::onPageChanged(int year, int month) const
{
    const QDate startDay{year, month, 1};
    const auto startDate = QDateTime{startDay, QTime{0, 0}};
    const auto endDate = QDateTime{startDay.addMonths(1), QTime{0, 0}};
    for(const vector<Session> foundSessions = sessionService_->getSessionsInTimeRange(startDate, endDate); const auto&
        session : foundSessions)
    {
        const QDate date = session.startTimestamp.date();

        QTextCharFormat format;
        format.setBackground(QBrush{Qt::red});
        format.setForeground(QBrush{Qt::white});

        ui->calendarWidget->setDateTextFormat(date, format);
    }
}
