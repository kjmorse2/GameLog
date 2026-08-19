#include "CalendarView.h"

#include <QTextCharFormat>

#include "ui_calendarview.h"

using gamelog::application::services::GameService;
using gamelog::application::services::SessionService;


namespace gamelog::gui
{
    CalendarView::CalendarView(QWidget* parent, GameService* gameService, SessionService* sessionService)
        : QWidget(parent),
          ui(new Ui::CalendarView),
          gameService_(gameService),
          sessionService_(sessionService)
    {
        ui->setupUi(this);
        calendar_ = ui->calendarWidget;
        connect(calendar_, &QCalendarWidget::currentPageChanged, this, &CalendarView::onPageChanged);

        // Populate the initially shown month directly rather than emitting the
        // calendar's own signal from outside the class.
        onPageChanged(calendar_->yearShown(), calendar_->monthShown());
    }

    CalendarView::~CalendarView() { delete ui; }

    void CalendarView::onPageChanged(int year, int month) const
    {
        const QDate startDay{year, month, 1};
        const auto startDate = QDateTime{startDay, QTime{0, 0}};
        const auto endDate = QDateTime{startDay.addMonths(1), QTime{0, 0}};
        for(const std::vector<gamelog::core::domain::Session> foundSessions = sessionService_->getSessionsInTimeRange(startDate, endDate); const auto&
            session : foundSessions)
        {
            const QDate date = session.startTimestamp.date();

            QTextCharFormat format;
            format.setBackground(QBrush{Qt::red});
            format.setForeground(QBrush{Qt::white});

            ui->calendarWidget->setDateTextFormat(date, format);
        }
    }
} // namespace gamelog::gui
