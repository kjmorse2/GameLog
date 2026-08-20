//
// Created by kj on 8/20/26.
//

#ifndef GAMELOG_SECRETDIALOG_H
#define GAMELOG_SECRETDIALOG_H

#include <QDialog>


QT_BEGIN_NAMESPACE

namespace Ui
{
    class SecretDialog;
}

QT_END_NAMESPACE

class SecretDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SecretDialog(const QString& title,
                           const QString& explanation,
                           const QString& placeholder,
                           QWidget* parent);
    ~SecretDialog() override;

    [[nodiscard]] QString secret() const;

private:
    Ui::SecretDialog* ui;
};


#endif //GAMELOG_SECRETDIALOG_H
