#pragma once

#include <QMainWindow>

class QLabel;
class QListWidget;

namespace gamelog::application {
class GameLogRuntime;
}

namespace gamelog::gui {

/** Minimal GUI shell that queries the in-process runtime directly. */
class MainWindow final : public QMainWindow
{
public:
    /**
     * @brief Constructs a new MainWindow.
     * @param runtime
     * @param parent
     */
    explicit MainWindow( application::GameLogRuntime &runtime, QWidget *parent = nullptr);

private:
    application::GameLogRuntime &runtime_;
    QLabel *statusLabel_{nullptr};
};

} // namespace gamelog::gui
