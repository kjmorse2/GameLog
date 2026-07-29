#pragma once

#include <QLoggingCategory>

/**
 * @brief Logging categories shared by the core, agent, GUI, database, and IPC layers.
 */
Q_DECLARE_LOGGING_CATEGORY(gamelogCoreLog)
Q_DECLARE_LOGGING_CATEGORY(gamelogAgentLog)
Q_DECLARE_LOGGING_CATEGORY(gamelogGuiLog)
Q_DECLARE_LOGGING_CATEGORY(gamelogDatabaseLog)
Q_DECLARE_LOGGING_CATEGORY(gamelogIpcLog)
