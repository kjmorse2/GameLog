//
// Created by kj on 8/6/26.
//

#ifndef GAMELOG_GAMECARD_H
#define GAMELOG_GAMECARD_H

#include <QWidget>
#include <application/services/web/GameArtworkService.h>

#include "core/domain/Game.h"


QT_BEGIN_NAMESPACE

namespace Ui
{
    class GameCard;
}

QT_END_NAMESPACE

class GameCard : public QWidget
{
    Q_OBJECT

public:
    /**
     * A visual Game Card widget.
     * @param parent The QWidget parent.
     * @param game The Game object to construct from.
     */
    explicit GameCard(QWidget* parent = nullptr, const gamelog::core::domain::Game& game = emptyGameCard);

    ~GameCard() override;

    /**
     * Returns the recommended size for the widget.
     * @return
     */
    [[nodiscard]] QSize sizeHint() const override;

    /**
     * Returns the minimum size for the widget.
     * @return
     */
    [[nodiscard]] QSize minimumSizeHint() const override;

    /**
     * Returns whether the widget has a height for a given width.
     * @return
     */
    [[nodiscard]] bool hasHeightForWidth() const override;

    /**
     * Returns the height for the given width.
     * @param width The width to get height for.
     * @return The calculated height for the width.
     */
    [[nodiscard]] int heightForWidth(int width) const override;

private:
    Ui::GameCard* ui;
    inline static const Game emptyGameCard = {
        .id = -1, .title = QStringLiteral(""), .executablePath = QStringLiteral(""),
        .executableName = QStringLiteral(""), .steamAppId = -1, .hasArtwork = false, .trackingEnabled = false
    };
};


#endif // GAMELOG_GAMECARD_H
