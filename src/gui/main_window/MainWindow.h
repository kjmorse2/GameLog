#pragma once

#include <QMainWindow>

namespace gamelog::gui::main_window
{
class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);
};
} // namespace gamelog::gui::main_window
