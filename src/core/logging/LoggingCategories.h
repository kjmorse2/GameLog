#pragma once

#include <QLoggingCategory>

/** Logging categories shared by the core, runtime, GUI, and database layers. */
Q_DECLARE_LOGGING_CATEGORY (gamelogCoreLog)
Q_DECLARE_LOGGING_CATEGORY (gamelogAgentLog)
Q_DECLARE_LOGGING_CATEGORY (gamelogGuiLog)
Q_DECLARE_LOGGING_CATEGORY (gamelogDatabaseLog)
