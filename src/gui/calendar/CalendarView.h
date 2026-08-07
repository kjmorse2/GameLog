//
// Created by kj on 8/7/26.
//

#ifndef GAMELOG_CALENDARVIEW_H
#define GAMELOG_CALENDARVIEW_H

#include <QWidget>
#include <QCalendarWidget>

#include "application/services/GameService.h"
#include "application/services/SessionService.h"
using gamelog::application::services::GameService;
using gamelog::application::services::SessionService;

QT_BEGIN_NAMESPACE
namespace Ui {
    class CalendarView;
}
QT_END_NAMESPACE

class CalendarView : public QWidget
{
    Q_OBJECT

public:
    explicit CalendarView(QWidget *parent = nullptr, GameService *gameService = nullptr, SessionService *sessionService = nullptr);
    ~CalendarView() override;

private:
    Ui::CalendarView *ui{};
    GameService *gameService_{};
    SessionService *sessionService_{};
    QCalendarWidget *calendar_;
private slots:
    void onPageChanged(int year, int month);
};


#endif // GAMELOG_CALENDARVIEW_H
