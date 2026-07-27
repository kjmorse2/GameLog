# GameLog Architecture (Scaffold)

GameLog is structured around two executables and one shared static library:

- `gamelog-agent` (Qt Core): planned owner of session lifecycle and background detection.
- `gamelog` (Qt Widgets): planned owner of presentation, browsing, and session note editing.
- `gamelog-core`: shared domain models, interfaces, and foundational services.

## Planned runtime responsibilities

- Exactly one active session at a time.
- Agent will eventually detect game processes and manage session state.
- GUI will eventually display/edit sessions and notes.
- Agent and GUI will eventually communicate through local IPC.

## Planned infrastructure

- SQLite storage via Qt SQL.
- Local IPC based on `QLocalServer`/`QLocalSocket` with length-prefixed UTF-8 JSON messages.

This document intentionally summarizes only scaffold-level architecture.
