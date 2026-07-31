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
#include "database/DatabaseManager.h"
#include "gui/MainWindow.h"
#include "logging/LoggingCategories.h"

namespace {

enum class RunMode
{
    Headless,
    Gui
};

bool hasArgument(int argc, char *argv[], const char *argument)
{
    for (int index = 1; index < argc; ++index)
    {
        if (std::strcmp(argv[index], argument) == 0)
        {
            return true;
        }
    }

    return false;
}

std::optional<RunMode> determineRunMode(int argc, char *argv[])
{
    const bool headless = hasArgument(argc, argv, "--headless");
    const bool gui = hasArgument(argc, argv, "--gui");

    if (headless && gui)
    {
        return std::nullopt;
    }

    // Launching gamelog directly is equivalent to --gui.
    return headless ? RunMode::Headless : RunMode::Gui;
}

QString runtimeLockPath()
{
    QString runtimeDirectory =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);

    if (runtimeDirectory.isEmpty())
    {
        runtimeDirectory =
            QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    QDir directory{runtimeDirectory};

    if (!directory.mkpath(QStringLiteral("gamelog")))
    {
        return {};
    }

    return directory.filePath(QStringLiteral("gamelog/runtime.lock"));
}

} // namespace

int main(int argc, char *argv[])
{
    const std::optional<RunMode> mode = determineRunMode(argc, argv);

    if (!mode)
    {
        qCritical("Use either --headless or --gui, not both.");
        return EXIT_FAILURE;
    }

    std::unique_ptr<QCoreApplication> application;

    if (*mode == RunMode::Gui)
    {
        application = std::make_unique<QApplication>(argc, argv);
    }
    else
    {
        application = std::make_unique<QCoreApplication>(argc, argv);
    }

    QCoreApplication::setOrganizationName(QStringLiteral("GameLog"));
    QCoreApplication::setApplicationName(QStringLiteral("GameLog"));

    const QString lockPath = runtimeLockPath();

    if (lockPath.isEmpty())
    {
        qCCritical(gamelogCoreLog)
            << "Failed to determine the GameLog runtime lock path.";
        return EXIT_FAILURE;
    }

    QLockFile runtimeLock{lockPath};
    runtimeLock.setStaleLockTime(0);

    if (!runtimeLock.tryLock(0))
    {
        qCCritical(gamelogCoreLog)
            << "Another GameLog runtime already owns tracking and the database.";
        return EXIT_FAILURE;
    }

    const QString databasePath =
        gamelog::core::database::DatabaseManager::resolveDatabasePath();

    if (databasePath.isEmpty())
    {
        qCCritical(gamelogDatabaseLog)
            << "Failed to determine database path.";
        return EXIT_FAILURE;
    }

    gamelog::application::GameLogRuntime runtime{databasePath};

    QObject::connect(
        application.get(),
        &QCoreApplication::aboutToQuit,
        [&runtime] { runtime.stop(); });

    constexpr std::chrono::seconds updateInterval{5};

    QTimer updateTimer;
    updateTimer.setInterval(
        static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                updateInterval)
                .count()));

    QObject::connect(
        &updateTimer,
        &QTimer::timeout,
        [&runtime, updateInterval] {
            runtime.update(updateInterval);
        });

    if (!runtime.start())
    {
        qCCritical(gamelogAgentLog)
            << "Failed to start the GameLog runtime.";
        return EXIT_FAILURE;
    }

    std::unique_ptr<gamelog::gui::MainWindow> mainWindow;

    if (*mode == RunMode::Gui)
    {
        mainWindow =
            std::make_unique<gamelog::gui::MainWindow>(runtime);
        mainWindow->show();
    }
    else
    {
        qCInfo(gamelogAgentLog) << "Running in headless mode.";
    }

    updateTimer.start();
    return application->exec();
}
