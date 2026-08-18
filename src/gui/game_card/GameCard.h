//
// Created by kj on 8/6/26.
//

#ifndef GAMELOG_GAMECARD_H
#define GAMELOG_GAMECARD_H

#include <QWidget>

#include "application/services/web/GameArtworkService.h"
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
     *
     * The optional artwork service preserves simple placeholder-only use while
     * allowing library cards to queue missing artwork and refresh asynchronously.
     * @param parent The QWidget parent.
     * @param game The Game object to construct from.
     * @param artworkService Service used to locate/download artwork, or nullptr.
     */
    explicit GameCard(QWidget* parent = nullptr,
                      const gamelog::core::domain::Game& game = emptyGameCard,
                      gamelog::application::services::GameArtworkService* artworkService = nullptr);

    ~GameCard() override;

    /**
     * Returns the recommended size for the widget.
     * @return The recommended widget size.
     */
    [[nodiscard]] QSize sizeHint() const override;

    /**
     * Returns the minimum size for the widget.
     * @return The minimum widget size.
     */
    [[nodiscard]] QSize minimumSizeHint() const override;

    /**
     * Returns whether the widget has a height for a given width.
     */
    [[nodiscard]] bool hasHeightForWidth() const override;

    /**
     * Returns the height for the given width.
     * @param width The width to get height for.
     * @return The calculated height for the width.
     */
    [[nodiscard]] int heightForWidth(int width) const override;

private:
    /**
     * Reloads cover.jpg for this card or displays the placeholder if unavailable.
     */
    void refreshArtwork();

    Ui::GameCard* ui{};
    int gameId_{-1};
    gamelog::application::services::GameArtworkService* artworkService_{nullptr};

    inline static const gamelog::core::domain::Game emptyGameCard{
        .id = -1, .title = QStringLiteral(""), .executablePath = QStringLiteral(""),
        .executableName = QStringLiteral(""), .steamAppId = std::nullopt, .hasArtwork = false, .trackingEnabled = false
    };
};

#endif // GAMELOG_GAMECARD_H
