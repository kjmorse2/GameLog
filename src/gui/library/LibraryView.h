#pragma once

#include <QWidget>
#include <gui/game_card/GameCard.h>

#include "application/services/local/GameService.h"
#include "domain/Game.h"

namespace gamelog::application
{
    class GameLogRuntime;
}

QT_BEGIN_NAMESPACE

namespace Ui
{
    class LibraryView;
}

QT_END_NAMESPACE

namespace gamelog::gui
{
    class LibraryView : public QWidget
    {
        Q_OBJECT

    public:
        /**
         * Constructs a LibraryView.
         *
         * @pre The runtime must have been started successfully. The windows
         * dereference GameLogRuntime's service accessors without null checks,
         * and those return nullptr when database initialization failed.
         * main.cpp enforces this by exiting when start() returns false.
         * @param parent The parent widget.
         * @param service The started runtime to query Games through.
         */
        explicit LibraryView(QWidget* parent = nullptr, gamelog::application::GameLogRuntime* service = nullptr);

        ~LibraryView() override;

    public
        Q_SLOTS :
        /**
         * Displays all games in the library.
         */
        void displayAllGames();

        void resizeEvent(QResizeEvent* event) override;

        /**
         * Watches the scroll area's viewport for resizes.
         *
         * The viewport is resized after this widget's own resize event has
         * already been delivered, so its width is what the grid must follow;
         * reading it from resizeEvent() alone would leave the column count one
         * resize behind.
         */
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        /**
         * @brief UI pointer from QT
         */
        Ui::LibraryView* ui{};

        /**
         * @brief The list of games on display in the library.
         */
        std::vector<core::domain::Game> gameList_;

        /**
         * @brief The Game service to query Games for.
         */
        application::GameLogRuntime* runtime_;

        /**
         * @brief The cards on display, owned by this view rather than by the
         * grid layout, so a re-layout can move them without rebuilding them.
         */
        std::vector<GameCard*> gameCards_;

        /**
         * @brief The spacing between game cards in the grid layout.
         */
        const int gridSpacing_ = 12;
        int numColumns_ = 4;

        /**
         * @brief Recomputes how many cards fit across the viewport.
         * @return True when the count changed and the grid needs re-laying out.
         */
        [[nodiscard]] bool calculateNewColumns();

        /**
         * @brief Width the grid may fill, excluding the vertical scroll bar.
         */
        [[nodiscard]] int availableGridWidth() const;

        /**
         * @brief Removes every card from the grid without destroying it.
         */
        void clearGrid();

        /**
         * @brief Re-places the existing cards using the current column count.
         */
        void relayoutGrid();
    };
} // namespace gamelog::gui
