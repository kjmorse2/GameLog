# GameLog

GameLog is a Linux-first desktop application scaffold for locally tracking PC gaming sessions.

## Current status

This repository currently contains the initial project scaffold only. Most planned features are represented by stubs
with TODOs and are intentionally unimplemented.

## Planned architecture

- `gamelog-core` static library shared by all executables.
- `gamelog-agent` Qt Core background executable.
- `gamelog` Qt Widgets desktop executable.
- Planned local IPC via Qt local sockets.
- Planned SQLite-backed persistence.

See `docs/architecture.md` for a short overview.

## Dependencies

- C++23 compiler (GCC or Clang)
- CMake
- Qt 6 development packages (Core, Widgets, Sql, Network, Test)

Example Arch Linux package names:

- `cmake`
- `qt6-base`
- `qt6-tools` (if your environment separates Qt tooling)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run

```bash
./build/src/agent/gamelog-agent
./build/src/gui/gamelog
```

## Repository layout

- `src/core`: shared domain and service stubs
- `src/agent`: background agent executable
- `src/gui`: Qt Widgets executable
- `tests`: starter Qt Test smoke test
- `docs`: architecture notes
- `resources`: placeholder icons and migration directories
- `packaging`: placeholder packaging/service files

## Roadmap

Next milestones include process detection, session lifecycle management, SQLite schema/data access, Steam import, local
IPC protocol, and richer GUI workflows.
