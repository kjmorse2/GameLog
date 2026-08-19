# GameLog Architecture

This scaffold note has been superseded.

The authoritative description of GameLog's architecture is
[GameLog_Software_Design_Document.md](GameLog_Software_Design_Document.md), which is kept
implementation-aligned. Behavioral contracts that the test suite enforces are listed in
[CONTRACT_CHANGES.md](CONTRACT_CHANGES.md).

For orientation, the current shape of the system is:

- **One executable, `gamelog`**, with three run modes selected by a single argument:
  `--headless`, `--gui`, or `--live`. There is no separate runtime process and no IPC.
- **`GameLogRuntime`** is the composition root. It owns the database connection, both
  repositories, and all five services, and drives process polling on a 5-second timer.
- **Layering** runs GUI to services to repositories to SQLite. Domain and query structs
  (`Game`, `Session`, `GameQuery`, `SessionQuery`) cross those boundaries; SQL does not
  leave the repository layer.
- **`gamelog-core`** holds domain, database, process, and logging code; `gamelog-application`
  holds the services and the runtime; `gamelog-gui` holds the Qt Widgets layer.

An earlier draft of this file described two executables (`gamelog-runtime` and `gamelog`)
communicating over local IPC. That design was never implemented and should not be used as a
reference.
