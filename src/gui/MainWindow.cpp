#include "gui/MainWindow.h"

#include "application/GameLogRuntime.h"
#include "domain/Game.h"
#include "domain/Session.h"
#include "library/LibraryView.h"

#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace gamelog::gui {

MainWindow::MainWindow(
    application::GameLogRuntime &runtime,
    QWidget *parent)
    : QMainWindow{parent}, runtime_{runtime}
{
    setWindowTitle(QStringLiteral("GameLog"));
    resize(720, 480);

    auto *centralWidget = new QWidget{this};
    auto *layout = new QVBoxLayout{centralWidget};

    auto *titleLabel = new QLabel{QStringLiteral("GameLog library"), centralWidget};
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *libraryViewWidget = new LibraryView{centralWidget, runtime.getGameService()};

    statusLabel_ = new QLabel{centralWidget};

    layout->addWidget(titleLabel);
    layout->addWidget(statusLabel_);
    layout->addWidget(libraryViewWidget);

    setCentralWidget(centralWidget);
}


} // namespace gamelog::gui
