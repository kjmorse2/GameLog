#include "AddGameDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QPushButton>

#include "ui_addgamedialog.h"

using gamelog::application::services::GameService;
using gamelog::core::domain::Game;

namespace gamelog::gui
{
    AddGameDialog::AddGameDialog(GameService& gameService, QWidget* parent)
        : QDialog{parent},
          ui{new Ui::AddGameDialog},
          gameService_{gameService}
    {
        ui->setupUi(this);

        ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Add"));
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

        connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AddGameDialog::accept);
        connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &AddGameDialog::reject);

        connect(ui->browseButton, &QToolButton::clicked, this, &AddGameDialog::onBrowseForExecutable);
        connect(ui->executablePathEdit, &QLineEdit::textChanged, this, &AddGameDialog::onExecutablePathChanged);
        connect(ui->titleEdit, &QLineEdit::textChanged, this, &AddGameDialog::onTitleChanged);

        // textEdited fires for typing only, so programmatic basename fills do not
        // count as the user taking over the field.
        connect(ui->executableNameEdit, &QLineEdit::textEdited, this, [this] { executableNameEdited_ = true; });

        connect(ui->steamAppIdCheckBox, &QCheckBox::toggled, ui->steamAppIdSpinBox, &QSpinBox::setEnabled);
    }

    AddGameDialog::~AddGameDialog() { delete ui; }

    const Game& AddGameDialog::createdGame() const noexcept { return createdGame_; }

    void AddGameDialog::accept()
    {
        std::optional<Game> game = gameFromForm();
        if(!game) { return; }

        if(!gameService_.addGame(*game))
        {
            showError(tr("The game could not be saved. Check the log for details."));
            return;
        }

        createdGame_ = *game;
        QDialog::accept();
    }

    std::optional<Game> AddGameDialog::gameFromForm()
    {
        const QString title = ui->titleEdit->text().trimmed();
        if(title.isEmpty())
        {
            showError(tr("A title is required."));
            return std::nullopt;
        }

        Game game;
        game.title = title;
        game.executablePath = ui->executablePathEdit->text().trimmed();
        game.executableName = ui->executableNameEdit->text().trimmed();
        game.trackingEnabled = ui->trackingEnabledCheckBox->isChecked();

        // Artwork is discovered separately; a freshly registered game never has a
        // local cover yet.
        game.hasArtwork = false;

        if(ui->steamAppIdCheckBox->isChecked()) { game.steamAppId = ui->steamAppIdSpinBox->value(); }

        showError({});
        return game;
    }

    void AddGameDialog::showError(const QString& message) { ui->errorLabel->setText(message); }

    void AddGameDialog::onBrowseForExecutable()
    {
        const QString path = QFileDialog::getOpenFileName(this,
                                                          tr("Select Game Executable"),
                                                          ui->executablePathEdit->text());

        if(path.isEmpty()) { return; }

        ui->executablePathEdit->setText(path);
    }

    void AddGameDialog::onExecutablePathChanged(const QString& path)
    {
        if(executableNameEdited_) { return; }

        ui->executableNameEdit->setText(QFileInfo{path.trimmed()}.fileName());
    }

    void AddGameDialog::onTitleChanged(const QString& title)
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!title.trimmed().isEmpty());
    }
} // namespace gamelog::gui
