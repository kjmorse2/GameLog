#pragma once

#include <QWidget>

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

    private:
        /**
         * @brief UI pointer from QT
         */
        Ui::LibraryView* ui{};

        /**
         * @brief The list of games on display in the library.
         */
        std::vector<gamelog::core::domain::Game> gameList_;

        /**
         * @brief The Game service to query Games for.
         */
        gamelog::application::GameLogRuntime* runtime_;
    };
} // namespace gamelog::gui
