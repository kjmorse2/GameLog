#pragma once

#include <QWidget>

namespace gamelog::gui::active_session
{
class ActiveSessionPanel final : public QWidget
{
public:
    explicit ActiveSessionPanel(QWidget *parent = nullptr);
};
} // namespace gamelog::gui::active_session
