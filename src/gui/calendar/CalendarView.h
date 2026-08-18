//
// Created by kj on 8/7/26.
//

#ifndef GAMELOG_CALENDARVIEW_H
#define GAMELOG_CALENDARVIEW_H

#include <QCalendarWidget>
#include <QWidget>

#include "../../application/services/local/GameService.h"
#include "../../application/services/local/SessionService.h"
using gamelog::application::services::GameService;
using gamelog::application::services::SessionService;

QT_BEGIN_NAMESPACE namespace Ui
{
    class CalendarView;
}

QT_END_NAMESPACE class CalendarView : public QWidget
{
    Q_OBJECT public:
    /**
     * Creates a new CalendarView.
     * @param parent The parent widget.
     * @param gameService The game service to query/edit for games.
     * @param sessionService The session service to querey/edit  for sessions
     */
    explicit CalendarView(QWidget* parent = nullptr,
                          GameService* gameService = nullptr,
                          SessionService* sessionService = nullptr);

    ~CalendarView() override;

private:
    /**
     * @brief ui pointer from QT
     */
    Ui::CalendarView* ui{};

    /**
     * @brief The game service to query/edit for games.
     */
    GameService* gameService_{};

    /**
     * @brief the session service to query/edit for sessions
     */
    SessionService* sessionService_{};

    /**
     * @brief The calendar widget.
     */
    QCalendarWidget* calendar_;

private slots :
    /**
     * Called when the calendar page is changed.
     * @param year The year the calendar is on
     * @param month The month the calendar is on.
     */
    void onPageChanged(int year, int month) const;
};


#endif // GAMELOG_CALENDARVIEW_H
