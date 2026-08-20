#include "AddSessionDialog.h"

#include <algorithm>
#include <chrono>

#include <QDateTime>
#include <QDialogButtonBox>
#include <QPushButton>

#include "ui_addsessiondialog.h"

using gamelog::application::services::GameService;
using gamelog::application::services::SessionService;
using gamelog::core::domain::Game;
using gamelog::core::domain::Session;
using gamelog::core::domain::SessionSource;
using gamelog::core::domain::SessionStatus;

namespace gamelog::gui
{
    namespace
    {
        /// Seconds in one minute, the unit the duration spin box is expressed in.
        constexpr qint64 kSecondsPerMinute = 60;
    } // namespace

    AddSessionDialog::AddSessionDialog(const GameService& gameService,
                                       SessionService& sessionService,
                                       QWidget* parent)
        : QDialog{parent},
          ui{new Ui::AddSessionDialog},
          sessionService_{sessionService}
    {
        ui->setupUi(this);

        ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Add"));

        const QDateTime now = QDateTime::currentDateTime();
        ui->startDateTimeEdit->setDateTime(now.addSecs(-3600));
        ui->endDateTimeEdit->setDateTime(now);

        populateGames(gameService);
        populateEnums();
        onTimesChanged();
        onStatusChanged();

        connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AddSessionDialog::accept);
        connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &AddSessionDialog::reject);

        connect(ui->startDateTimeEdit, &QDateTimeEdit::dateTimeChanged, this, &AddSessionDialog::onTimesChanged);
        connect(ui->endDateTimeEdit, &QDateTimeEdit::dateTimeChanged, this, &AddSessionDialog::onTimesChanged);
        connect(ui->statusComboBox, &QComboBox::currentIndexChanged, this, &AddSessionDialog::onStatusChanged);
    }

    AddSessionDialog::~AddSessionDialog() { delete ui; }

    const Session& AddSessionDialog::createdSession() const noexcept { return createdSession_; }

    void AddSessionDialog::accept()
    {
        std::optional<Session> session = sessionFromForm();
        if(!session) { return; }

        if(!sessionService_.addSession(*session))
        {
            showError(tr("The session could not be saved. A second active session is refused; "
                         "check the log for other causes."));
            return;
        }

        createdSession_ = *session;
        QDialog::accept();
    }

    void AddSessionDialog::populateGames(const GameService& gameService)
    {
        for(const Game& game : gameService.listGames()) { ui->gameComboBox->addItem(game.title, game.id); }

        if(ui->gameComboBox->count() != 0) { return; }

        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        showError(tr("Add a game before recording a session for it."));
    }

    void AddSessionDialog::populateEnums()
    {
        for(const SessionStatus status : {SessionStatus::Completed, SessionStatus::Interrupted, SessionStatus::Active})
        {
            ui->statusComboBox->addItem(toDisplayString(status), static_cast<int>(status));
        }

        for(const SessionSource source : {SessionSource::Manual, SessionSource::Automatic})
        {
            ui->sourceComboBox->addItem(toDisplayString(source), static_cast<int>(source));
        }
    }

    std::optional<Session> AddSessionDialog::sessionFromForm()
    {
        if(ui->gameComboBox->currentIndex() < 0)
        {
            showError(tr("Select a game for this session."));
            return std::nullopt;
        }

        const QDateTime start = ui->startDateTimeEdit->dateTime();
        const QDateTime end = ui->endDateTimeEdit->dateTime();
        const SessionStatus status = selectedStatus();

        if(status != SessionStatus::Active && end < start)
        {
            showError(tr("The end time cannot precede the start time."));
            return std::nullopt;
        }

        Session session;
        session.gameId = ui->gameComboBox->currentData().toInt();
        session.startTimestamp = start.toUTC();
        session.status = status;
        session.source = static_cast<SessionSource>(ui->sourceComboBox->currentData().toInt());
        session.trackedDuration = std::chrono::seconds{ui->durationSpinBox->value() * kSecondsPerMinute};
        session.notes = ui->notesEdit->toPlainText();

        // Active rows are persisted without an end timestamp; every other status
        // requires one.
        if(status != SessionStatus::Active) { session.endTimestamp = end.toUTC(); }

        showError({});
        return session;
    }

    SessionStatus AddSessionDialog::selectedStatus() const
    {
        return static_cast<SessionStatus>(ui->statusComboBox->currentData().toInt());
    }

    void AddSessionDialog::showError(const QString& message) { ui->errorLabel->setText(message); }

    void AddSessionDialog::onTimesChanged()
    {
        if(selectedStatus() == SessionStatus::Active) { return; }

        const qint64 elapsedSeconds = ui->startDateTimeEdit->dateTime().secsTo(ui->endDateTimeEdit->dateTime());
        const qint64 minutes = std::clamp<qint64>(elapsedSeconds / kSecondsPerMinute,
                                                  ui->durationSpinBox->minimum(),
                                                  ui->durationSpinBox->maximum());

        ui->durationSpinBox->setValue(static_cast<int>(minutes));
    }

    void AddSessionDialog::onStatusChanged()
    {
        const bool active = selectedStatus() == SessionStatus::Active;

        ui->endDateTimeEdit->setEnabled(!active);
        ui->endLabel->setEnabled(!active);

        if(active) { ui->durationSpinBox->setValue(0); }
        else { onTimesChanged(); }
    }
} // namespace gamelog::gui
