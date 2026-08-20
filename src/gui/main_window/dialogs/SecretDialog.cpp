//
// Created by kj on 8/20/26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_SecretDialog.h" resolved

#include "gui/main_window/dialogs/SecretDialog.h"

#include <QDialogButtonBox>
#include <QPushButton>

#include "ui_secretdialog.h"


SecretDialog::SecretDialog(const QString& title,
                           const QString& explanation,
                           const QString& placeholder,
                           QWidget* parent)
    : QDialog{parent},
      ui{new Ui::SecretDialog}
{
    ui->setupUi(this);

    setWindowTitle(title);

    ui->explanationLabel->setText(explanation);
    ui->secretEdit->setPlaceholderText(placeholder);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Submit"));
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, [this]
    {
        this->reject();
        this->close();
    });
    connect(ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, [this]
    {
        this->QDialog::accept();
        this->close();
    });
}
QString SecretDialog::secret() const
{
    return ui->secretEdit->text().trimmed();
}
SecretDialog::~SecretDialog() { delete ui; }
