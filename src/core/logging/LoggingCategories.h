#pragma once

#include <QLoggingCategory>

/** Logging categories shared by the core, runtime, GUI, and database layers. */
Q_DECLARE_LOGGING_CATEGORY (gamelogCoreLog)

Q_DECLARE_LOGGING_CATEGORY (gamelogRuntimeLog)

Q_DECLARE_LOGGING_CATEGORY (gamelogGuiLog)

Q_DECLARE_LOGGING_CATEGORY (gamelogDatabaseLog)

Q_DECLARE_LOGGING_CATEGORY (gamelogProcessLog)

Q_DECLARE_LOGGING_CATEGORY (gamelogGameServiceLog)

Q_DECLARE_LOGGING_CATEGORY (gamelogSessionServiceLog)

Q_DECLARE_LOGGING_CATEGORY (gamelogSteamApiServiceLog)
