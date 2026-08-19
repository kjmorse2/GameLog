#pragma once

#include <QCalendarWidget>
#include <QWidget>

#include "application/services/local/GameService.h"
#include "application/services/local/SessionService.h"

QT_BEGIN_NAMESPACE

namespace Ui
{
    class CalendarView;
}

QT_END_NAMESPACE

namespace gamelog::gui
{
    class CalendarView : public QWidget
    {
        Q_OBJECT

    public:
        /**
         * Creates a new CalendarView.
         *
         * @pre sessionService must be non-null for the calendar to populate.
         * GameLogRuntime's accessors return nullptr when database
         * initialization failed, so callers must have started the runtime.
         * @param parent The parent widget.
         * @param sessionService The session service to query/edit for sessions.
         */
        explicit CalendarView(QWidget* parent = nullptr,
                              gamelog::application::services::SessionService* sessionService = nullptr);

        ~CalendarView() override;

    private:
        /**
         * @brief ui pointer from QT
         */
        Ui::CalendarView* ui{};

        /**
         * @brief the session service to query/edit for sessions
         */
        gamelog::application::services::SessionService* sessionService_{};

        /**
         * @brief The calendar widget.
         */
        QCalendarWidget* calendar_{};

    private Q_SLOTS:
        /**
         * Called when the calendar page is changed.
         * @param year The year the calendar is on
         * @param month The month the calendar is on.
         */
        void onPageChanged(int year, int month) const;
    };
} // namespace gamelog::gui
