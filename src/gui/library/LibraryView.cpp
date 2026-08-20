#include "LibraryView.h"

#include <algorithm>
#include <cstddef>

#include <QEvent>
#include <QObject>
#include <QScrollBar>
#include <QStyle>
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
        ui->gridLayout->setSpacing(gridSpacing_);
        ui->gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        // The cards live in the inner layout, so that is the one whose spacing
        // the column arithmetic assumes.
        ui->gameGridLayout->setContentsMargins(0, 0, 0, 0);
        ui->gameGridLayout->setSpacing(gridSpacing_);

        connect(ui->refreshButton, &QPushButton::clicked, this, &LibraryView::displayAllGames);
        connect(ui->syncSteamGamesButton,
                &QPushButton::clicked,
                runtime_->getGameService(),
                &GameService::syncSteamGames);

        ui->gameScrollArea->viewport()->installEventFilter(this);

        displayAllGames();
    }

    LibraryView::~LibraryView() { delete ui; }

    void LibraryView::displayAllGames()
    {
        // The cards are owned by this view, so empty the layout before deleting
        // them: a layout item must never outlive the widget it refers to.
        clearGrid();
        qDeleteAll(gameCards_);
        gameCards_.clear();

        for(const core::domain::Game& game : runtime_->getGameService()->listGames())
        {
            gameCards_.push_back(new GameCard{ui->gameGridContainer, game});
        }

        static_cast<void>(calculateNewColumns());
        relayoutGrid();
    }

    bool LibraryView::calculateNewColumns()
    {
        const int availableWidth = availableGridWidth();
        const int cardWidth = GameCard::cardSizeHint().width();

        const int columns = std::max(1, (availableWidth + gridSpacing_) / (cardWidth + gridSpacing_));
        if(columns == numColumns_) { return false; }

        numColumns_ = columns;
        return true;
    }

    int LibraryView::availableGridWidth() const
    {
        const QScrollArea* scrollArea = ui->gameScrollArea;
        int width = scrollArea->viewport()->width();

        // Reserve the vertical scroll bar's width even while it is hidden.
        // Otherwise a new row could bring the bar in, narrow the viewport, drop
        // a column, lose the row again, and oscillate forever.
        if(!scrollArea->verticalScrollBar()->isVisible())
        {
            width -= scrollArea->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, scrollArea);
        }

        return width;
    }

    void LibraryView::clearGrid()
    {
        // Deleting a QLayoutItem does not delete its widget; the cards stay
        // parented to the grid container and are re-placed below.
        while(QLayoutItem* item = ui->gameGridLayout->takeAt(0)) { delete item; }
    }

    void LibraryView::relayoutGrid()
    {
        clearGrid();

        for(std::size_t i = 0; i < gameCards_.size(); ++i)
        {
            const int index = static_cast<int>(i);

            ui->gameGridLayout->addWidget(gameCards_[i], index / numColumns_, index % numColumns_);
        }
    }

    void LibraryView::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);

        if(calculateNewColumns()) { relayoutGrid(); }
    }

    bool LibraryView::eventFilter(QObject* watched, QEvent* event)
    {
        if(watched == ui->gameScrollArea->viewport() && event->type() == QEvent::Resize && calculateNewColumns())
        {
            relayoutGrid();
        }

        return QWidget::eventFilter(watched, event);
    }
} // namespace gamelog::gui
