# GameLog Architecture (Scaffold)

GameLog is structured around two executables and one shared static library:

- `gamelog-runtime` (Qt Core): planned owner of session lifecycle and background detection.
- `gamelog` (Qt Widgets): planned owner of presentation, browsing, and session note editing.
- `gamelog-core`: shared domain models, interfaces, and foundational services.

## Planned runtime responsibilities

- Exactly one active session at a time.
- Runtime will eventually detect game processes and manage session state.
- GUI will eventually display/edit sessions and notes.
- Runtime and GUI will eventually communicate through local IPC.

## Planned infrastructure

- SQLite storage via Qt SQL.
- Automatic start/stop of listener when gui session starts, so it can run headless or with gui.
