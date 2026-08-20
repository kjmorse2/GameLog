#pragma once

#include <QWidget>

#include "application/services/web/GameArtworkService.h"
#include "core/domain/Game.h"

QT_BEGIN_NAMESPACE

namespace Ui
{
    class GameCard;
}

QT_END_NAMESPACE

namespace gamelog::gui
{
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
         * @param artworkService Service whose artworkAvailable signal triggers a
         * redraw, or nullptr. The card never requests a download itself: deciding
         * when to fetch artwork is application policy and belongs to the owner.
         */
        explicit GameCard(QWidget* parent = nullptr,
                          const gamelog::core::domain::Game& game = emptyGameCard,
                          gamelog::application::services::GameArtworkService* artworkService = nullptr);

        ~GameCard() override;

        /**
         * The fixed size every card asks for.
         *
         * Static so that layout code can size a grid against a card without
         * building one: constructing a card loads and scales its artwork.
         */
        [[nodiscard]] static QSize cardSizeHint() noexcept;

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
            .executableName = QStringLiteral(""), .steamAppId = std::nullopt, .hasArtwork = false,
            .trackingEnabled = false
        };
    };
} // namespace gamelog::gui
