#pragma once

#include <optional>

#include <QDialog>

#include "application/services/local/GameService.h"
#include "domain/Game.h"

QT_BEGIN_NAMESPACE

namespace Ui
{
    class AddGameDialog;
}

QT_END_NAMESPACE

namespace gamelog::gui
{
    /**
     * @brief Modal form that persists one new game.
     *
     * The dialog owns the whole submission: accepting it validates the entered
     * fields, inserts the row through GameService, and only then closes. A
     * rejected insert keeps the dialog open with the reason shown inline, so a
     * returned QDialog::Accepted always means a row was written.
     */
    class AddGameDialog final : public QDialog
    {
        Q_OBJECT

    public:
        /**
         * @brief Constructs the dialog around the service that will persist the game.
         * @param gameService The service used to insert the new game.
         * @param parent The parent widget.
         */
        explicit AddGameDialog(application::services::GameService& gameService, QWidget* parent = nullptr);

        ~AddGameDialog() override;

        /**
         * @brief The persisted game, carrying the ID assigned by the repository.
         *
         * Only meaningful after exec() returned QDialog::Accepted.
         */
        [[nodiscard]] const core::domain::Game& createdGame() const noexcept;

        /**
         * Validates the form and inserts the game. Overridden so that a failed
         * insert leaves the dialog open instead of reporting success.
         */
        void accept() override;

    private:
        /**
         * @brief Builds a game from the form, or std::nullopt when a field is invalid.
         *
         * The rejection reason is written to the inline error label.
         */
        [[nodiscard]] std::optional<core::domain::Game> gameFromForm();

        /**
         * @brief Shows an inline validation or persistence failure.
         */
        void showError(const QString& message);

    private
        Q_SLOTS :
        /**
         * Opens a file chooser for the executable path.
         */
        void onBrowseForExecutable();

        /**
         * Mirrors the chosen executable's basename into the name field until the
         * user edits that field themselves.
         */
        void onExecutablePathChanged(const QString& path);

        /**
         * Enables submission only while the required title is non-blank.
         */
        void onTitleChanged(const QString& title);

    private:
        Ui::AddGameDialog* ui;

        /**
         * @brief The service used to insert the new game.
         */
        application::services::GameService& gameService_;

        /**
         * @brief The game written on submission, valid once the dialog is accepted.
         */
        core::domain::Game createdGame_;

        /**
         * @brief True once the user typed in the executable name field, which
         * stops the field from being overwritten by the path's basename.
         */
        bool executableNameEdited_{false};
    };
} // namespace gamelog::gui
