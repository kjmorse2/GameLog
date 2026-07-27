#pragma once

namespace gamelog::gui::tray
{
class TrayController
{
public:
    void initialize();
    [[nodiscard]] bool isAvailable() const;
};
} // namespace gamelog::gui::tray
