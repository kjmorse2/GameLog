#pragma once

#include <QCalendarWidget>
#include <QWidget>

#include "application/services/local/GameService.h"
#include "application/services/local/SessionService.h"
#include "core/domain/query/SessionQuery.h"

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

    public
        Q_SLOTS :
        /**
         * Re-reads the sessions of the month currently on screen. Callers use
         * this after writing a session, since the calendar otherwise only
         * refreshes when its page changes.
         */
        void refresh();

        /**
         * Restricts the calendar to a single game's sessions.
         *
         * The filter persists across page changes: onPageChanged() only
         * rewrites the date bounds, so paging keeps showing this game.
         * @param game The game whose sessions the calendar should show.
         */
        void onGameSelected(const gamelog::core::domain::Game& game);

        void onGameSelectionCleared();

    private:
        /**
         * @brief ui pointer from QT
         */
        Ui::CalendarView* ui{};

        /**
         * @brief the session service to query/edit for sessions
         */
        application::services::SessionService* sessionService_{};

        /**
         * @brief The calendar widget.
         */
        QCalendarWidget* calendar_{};

        /**
         * @brief The search this view paints.
         *
         * Holding the criteria rather than the results keeps one query behind
         * both entry points: onPageChanged() overwrites the date bounds for
         * the month on screen, onGameSelected() narrows gameIds, and either
         * change repaints through the same path.
         */
        core::domain::query::SessionQuery sessionQuery_{};

    private
        Q_SLOTS :
        /**
         * Called when the calendar page is changed.
         * @param year The year the calendar is on
         * @param month The month the calendar is on.
         */
        void onPageChanged(int year, int month);
    };
} // namespace gamelog::gui
