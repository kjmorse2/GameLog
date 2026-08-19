#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>
#include <QTimer>

#include "application/GameLogRuntime.h"
#include "application/RunMode.h"
#include "database/DatabaseManager.h"
#include "gui/live_window/LiveWindow.h"
#include "gui/main_window/MainWindow.h"
#include "logging/LoggingCategories.h"

namespace
{
    /**
     * @brief Get the path to the GameLog runtime lock file.
     * @return The path to the lock file, or an empty string if it cannot be determined.
     */
    QString runtimeLockPath()
    {
        QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);

        if(runtimeDirectory.isEmpty())
        {
            runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        }

        const QDir directory{runtimeDirectory};

        if(!directory.mkpath(QStringLiteral("gamelog"))) { return {}; }

        return directory.filePath(QStringLiteral("gamelog/runtime.lock"));
    }
} // namespace

int main(int argc, char* argv[])
{
    using gamelog::application::RunMode;

    const std::optional<RunMode> mode = gamelog::application::determineRunMode(argc, argv);

    if(!mode)
    {
        qCCritical(gamelogCoreLog) << "Specify exactly one of --headless, --gui, or --live and no other arguments.";
        return EXIT_FAILURE;
    }

    std::unique_ptr<QCoreApplication> application;

    if(*mode == RunMode::Gui || *mode == RunMode::Live) { application = std::make_unique<QApplication>(argc, argv); }
    else { application = std::make_unique<QCoreApplication>(argc, argv); }

    QCoreApplication::setOrganizationName(QStringLiteral("GameLog"));
    QCoreApplication::setApplicationName(QStringLiteral("GameLog"));

    const QString lockPath = runtimeLockPath();

    if(lockPath.isEmpty())
    {
        qCCritical(gamelogCoreLog) << "Failed to determine the GameLog runtime lock path.";
        return EXIT_FAILURE;
    }

    QLockFile runtimeLock{lockPath};
    runtimeLock.setStaleLockTime(0);

    if(!runtimeLock.tryLock(0))
    {
        qCCritical(gamelogCoreLog) << "Another GameLog runtime already owns tracking and the database.";
        return EXIT_FAILURE;
    }

    const QString databasePath = gamelog::core::database::DatabaseManager::resolveDatabasePath();

    if(databasePath.isEmpty())
    {
        qCCritical(gamelogDatabaseLog) << "Failed to determine database path.";
        return EXIT_FAILURE;
    }

    gamelog::application::GameLogRuntime runtime{databasePath};

    QObject::connect(application.get(), &QCoreApplication::aboutToQuit, [&runtime] { runtime.stop(); });

    constexpr std::chrono::seconds updateInterval{5};

    QTimer updateTimer;
    updateTimer.setInterval(std::chrono::duration_cast<std::chrono::milliseconds>(updateInterval).count());

    QObject::connect(&updateTimer, &QTimer::timeout, [&runtime, updateInterval] { runtime.update(updateInterval); });

    if(!runtime.start())
    {
        qCCritical(gamelogRuntimeLog) << "Failed to start the GameLog runtime.";
        return EXIT_FAILURE;
    }

    std::unique_ptr<QMainWindow> mainWindow;

    if(*mode == RunMode::Gui)
    {
        mainWindow = std::make_unique<gamelog::gui::MainWindow>(runtime);
        mainWindow->show();
    }
    else if(*mode == RunMode::Live)
    {
        mainWindow = std::make_unique<gamelog::gui::LiveWindow>(runtime);
        mainWindow->show();
    }
    else { qCInfo(gamelogRuntimeLog) << "Running in headless mode."; }

    updateTimer.start();
    return application->exec();
}
