#include "gui/game_card/GameCard.h"

#include "resources/AppPaths.h"
#include "ui_gamecard.h"

#include <QDir>
#include <QPixmap>

using gamelog::application::services::ArtworkType;
using gamelog::application::services::GameArtworkService;
using gamelog::core::domain::Game;

namespace gamelog::gui
{
    GameCard::GameCard(QWidget* parent, const Game& game, GameArtworkService* artworkService)
        : QWidget{parent},
          ui{new Ui::GameCard},
          gameId_{game.id},
          artworkService_{artworkService}
    {
        ui->setupUi(this);

        ui->gameArtLabel->setAlignment(Qt::AlignCenter);
        ui->gameArtLabel->setScaledContents(false);
        ui->gameTitleLabel->setText(game.title);

        if(artworkService_ != nullptr)
        {
            connect(artworkService_,
                    &GameArtworkService::artworkAvailable,
                    this,
                    [this](int gameId, ArtworkType artworkType)
                    {
                        if(gameId == gameId_ && artworkType == ArtworkType::Cover) { refreshArtwork(); }
                    });
        }

        refreshArtwork();
    }

    GameCard::~GameCard() { delete ui; }

    QSize GameCard::cardSizeHint() noexcept { return {135, 240}; }

    QSize GameCard::sizeHint() const { return cardSizeHint(); }

    QSize GameCard::minimumSizeHint() const { return {90, 160}; }

    bool GameCard::hasHeightForWidth() const { return true; }

    int GameCard::heightForWidth(int width) const { return width * 9 / 16; }

    void GameCard::refreshArtwork()
    {
        QPixmap imageMap;

        if(gameId_ > 0)
        {
            const QString coverPath = QDir{gamelog::core::AppPaths::gameArtworkDirectory(gameId_)}.
                filePath(QStringLiteral("cover.jpg"));
            imageMap.load(coverPath);
        }

        if(imageMap.isNull()) { imageMap = QPixmap{QStringLiteral(":/images/GameArtPlaceholder.png")}; }

        imageMap = imageMap.scaled(ui->gameArtLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        ui->gameArtLabel->setPixmap(imageMap);
    }
} // namespace gamelog::gui
