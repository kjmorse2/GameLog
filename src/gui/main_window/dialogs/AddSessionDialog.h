#pragma once

#include <optional>

#include <QDialog>

#include "application/services/local/GameService.h"
#include "application/services/local/SessionService.h"
#include "domain/Session.h"

QT_BEGIN_NAMESPACE

namespace Ui
{
    class AddSessionDialog;
}

QT_END_NAMESPACE

namespace gamelog::gui
{
    /**
     * @brief Modal form that persists one new play session.
     *
     * Times are entered in local time and converted to UTC on submission, which
     * is the only form the domain persists. As with AddGameDialog, accepting
     * performs the insert, so QDialog::Accepted always means a row was written.
     */
    class AddSessionDialog final : public QDialog
    {
        Q_OBJECT

    public:
        /**
         * @brief Constructs the dialog around the services it reads and writes.
         * @param gameService Supplies the selectable games.
         * @param sessionService Persists the new session.
         * @param parent The parent widget.
         */
        explicit AddSessionDialog(const application::services::GameService& gameService,
                                  application::services::SessionService& sessionService,
                                  QWidget* parent = nullptr);

        ~AddSessionDialog() override;

        /**
         * @brief The persisted session, carrying the ID assigned by the repository.
         *
         * Only meaningful after exec() returned QDialog::Accepted.
         */
        [[nodiscard]] const core::domain::Session& createdSession() const noexcept;

        /**
         * Validates the form and inserts the session. Overridden so that a failed
         * insert leaves the dialog open instead of reporting success.
         */
        void accept() override;

    private:
        /**
         * @brief Fills the game selector, disabling submission when the library is empty.
         */
        void populateGames(const application::services::GameService& gameService);

        /**
         * @brief Fills the status and source selectors from the domain enums.
         */
        void populateEnums();

        /**
         * @brief Builds a session from the form, or std::nullopt when a field is invalid.
         *
         * The rejection reason is written to the inline error label.
         */
        [[nodiscard]] std::optional<core::domain::Session> sessionFromForm();

        /**
         * @brief The status currently chosen in the selector.
         */
        [[nodiscard]] core::domain::SessionStatus selectedStatus() const;

        /**
         * @brief Shows an inline validation or persistence failure.
         */
        void showError(const QString& message);

    private
        Q_SLOTS :
        /**
         * Recomputes the tracked duration from the entered start and end times.
         * The value stays editable afterwards so an estimate can be recorded.
         */
        void onTimesChanged();

        /**
         * Enables the end-time field only for statuses that require one; active
         * sessions must not carry an end timestamp.
         */
        void onStatusChanged();

    private:
        Ui::AddSessionDialog* ui;

        /**
         * @brief The service used to insert the new session.
         */
        application::services::SessionService& sessionService_;

        /**
         * @brief The session written on submission, valid once the dialog is accepted.
         */
        core::domain::Session createdSession_;
    };
} // namespace gamelog::gui
