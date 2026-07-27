#include "active_session/ActiveSessionPanel.h"

#include <QLabel>
#include <QVBoxLayout>

namespace gamelog::gui::active_session
{
ActiveSessionPanel::ActiveSessionPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    auto *stateLabel = new QLabel(QStringLiteral("No active session"), this);
    stateLabel->setAlignment(Qt::AlignCenter);

    auto *elapsedLabel = new QLabel(QStringLiteral("Elapsed: --:--:--"), this);
    elapsedLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(stateLabel);
    layout->addWidget(elapsedLabel);
}
} // namespace gamelog::gui::active_session
