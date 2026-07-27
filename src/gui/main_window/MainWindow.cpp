#include "main_window/MainWindow.h"

#include <QAction>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace gamelog::gui::main_window
{
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("GameLog"));
    resize(640, 360);

    auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    auto *exitAction = fileMenu->addAction(QStringLiteral("E&xit"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto *centralWidget = new QWidget(this);
    auto *layout = new QVBoxLayout(centralWidget);

    auto *welcomeLabel = new QLabel(QStringLiteral("Welcome to GameLog"), centralWidget);
    welcomeLabel->setAlignment(Qt::AlignCenter);

    layout->addStretch();
    layout->addWidget(welcomeLabel);
    layout->addStretch();

    setCentralWidget(centralWidget);
    statusBar()->showMessage(QStringLiteral("GameLog GUI started"));
}
} // namespace gamelog::gui::main_window
