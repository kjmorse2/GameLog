#include "LibraryView.h"

#include <cstddef>

#include <QObject>
#include <application/GameLogRuntime.h>
#include <application/services/local/GameService.h>
#include <application/services/web/GameArtworkService.h>

#include "gui/game_card/GameCard.h"

#include "ui_libraryview.h"

using gamelog::application::services::GameService;

namespace gamelog::gui
{
    LibraryView::LibraryView(QWidget* parent, gamelog::application::GameLogRuntime* runtime) : QWidget(parent),
        ui(new Ui::LibraryView),
        runtime_(runtime)
    {
        ui->setupUi(this);
        ui->gridLayout->setContentsMargins(0, 0, 0, 0);
        ui->gridLayout->setSpacing(12);
        ui->gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        connect(ui->refreshButton, &QPushButton::clicked, this, &LibraryView::displayAllGames);
        connect(ui->syncSteamGamesButton,
                &QPushButton::clicked,
                runtime_->getGameService(),
                &GameService::syncSteamGames);
        displayAllGames();
    }

    LibraryView::~LibraryView() { delete ui; }

    void LibraryView::displayAllGames()
    {
        const auto games = runtime_->getGameService()->listGames();
        while(const QLayoutItem* item = ui->gameGridLayout->takeAt(0))
        {
            if(QWidget* widget = item->widget())
            {
                widget->setParent(nullptr);
                delete widget;
            }
            delete item;
        }

        constexpr std::size_t columns = 4;

        for(std::size_t i = 0; i < games.size(); ++i)
        {
            const int row = static_cast<int>(i / columns);
            const int col = static_cast<int>(i % columns);

            auto* gameCard = new GameCard(ui->gameGridContainer, games[i]);
            ui->gameGridLayout->addWidget(gameCard, row, col);
        }
    }
} // namespace gamelog::gui
