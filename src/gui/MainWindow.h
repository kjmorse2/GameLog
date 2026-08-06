#ifndef GAMELOG_MAINWINDOW_H
#define GAMELOG_MAINWINDOW_H

#include <QWidget>
#include <QMainWindow>

class QLabel;
class QListWidget;

namespace gamelog::application {
class GameLogRuntime;
}

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

namespace gamelog::gui {

/** Minimal GUI shell that queries the in-process runtime directly. */
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a new MainWindow.
     * @param runtime
     * @param parent
     */
    explicit MainWindow(application::GameLogRuntime &runtime, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    application::GameLogRuntime &runtime_;
    QLabel *statusLabel_{nullptr};
};

} // namespace gamelog::gui
#endif
