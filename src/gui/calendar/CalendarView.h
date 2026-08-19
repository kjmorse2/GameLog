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
         * @param parent The parent widget.
         * @param gameService The game service to query/edit for games.
         * @param sessionService The session service to query/edit for sessions.
         */
        explicit CalendarView(QWidget* parent = nullptr,
                              gamelog::application::services::GameService* gameService = nullptr,
                              gamelog::application::services::SessionService* sessionService = nullptr);

        ~CalendarView() override;

    private:
        /**
         * @brief ui pointer from QT
         */
        Ui::CalendarView* ui{};

        /**
         * @brief The game service to query/edit for games.
         */
        gamelog::application::services::GameService* gameService_{};

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
