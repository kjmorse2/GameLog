#include "CalendarView.h"

#include <QTextCharFormat>

#include "ui_calendarview.h"

using gamelog::application::services::SessionService;


namespace gamelog::gui
{
    CalendarView::CalendarView(QWidget* parent, SessionService* sessionService)
        : QWidget(parent),
          ui(new Ui::CalendarView),
          sessionService_(sessionService)
    {
        ui->setupUi(this);
        calendar_ = ui->calendarWidget;
        connect(calendar_, &QCalendarWidget::currentPageChanged, this, &CalendarView::onPageChanged);

        // Populate the initially shown month directly rather than emitting the
        // calendar's own signal from outside the class.
        refresh();
    }

    CalendarView::~CalendarView() { delete ui; }

    void CalendarView::refresh()
    {
        onPageChanged(calendar_->yearShown(), calendar_->monthShown());
    }

    void CalendarView::onPageChanged(int year, int month)
    {
        const QDate startDay{year, month, 1};
        sessionQuery_.startedAtOrAfter = QDateTime{startDay, QTime{0, 0}};
        sessionQuery_.startedBefore = QDateTime{startDay.addMonths(1), QTime{0, 0}};

        // A null QDate clears every format in the table. Formats otherwise only
        // accumulate, so without this a narrowed filter leaves the days it
        // dropped still painted and a page change carries the old month's days.
        calendar_->setDateTextFormat(QDate{}, QTextCharFormat{});

        QTextCharFormat format;
        format.setBackground(QBrush{Qt::red});
        format.setForeground(QBrush{Qt::white});

        for(const auto& session : sessionService_->search(sessionQuery_))
        {
            calendar_->setDateTextFormat(session.startTimestamp.date(), format);
        }
    }

    void CalendarView::onGameSelected(const gamelog::core::domain::Game& game)
    {
        sessionQuery_.gameIds = {game.id};
        refresh();
    }

    void CalendarView::onGameSelectionCleared()
    {
        sessionQuery_.gameIds.clear();
        refresh();
    }
} // namespace gamelog::gui
