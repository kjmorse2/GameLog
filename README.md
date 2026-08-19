# GameLog

GameLog is a Linux desktop application for tracking PC gaming sessions locally. It detects running
games, records play sessions automatically, keeps per-session notes, and pulls your library and
artwork from Steam.

Everything is stored on your machine in a local SQLite database. Steam credentials are kept in the
system keychain, never in the database or in configuration files.

## Features

- **Automatic session tracking.** Polls running processes and starts a session once a tracked game
  has been running continuously for 30 seconds, ending it 30 seconds after the game disappears.
- **Process matching by Steam App ID or executable path**, with Steam identity read from the
  process environment.
- **Session notes** in Markdown, saved atomically with the session they belong to.
- **Steam library synchronization.** Adds owned games that are not in the database yet; existing
  rows are never overwritten, retitled, or re-enabled.
- **Artwork** (cover, header, logo) downloaded from the Steam CDN and validated before being stored.
- **Calendar and library views**, plus a compact live window for the session in progress.

## Status

Working and under active development. The backend — domain model, repositories, services, process
detection, migrations — is implemented and covered by an automated test suite. The GUI is functional
but is the least finished part of the project and has no automated tests yet.

## Building

Requirements:

- C++23 compiler (GCC or Clang)
- CMake 3.24 or newer
- Qt 6: Core, Gui, Widgets, Network, Sql, Test
- QtKeychain (Qt 6 build)
- libproc2 (from procps-ng)

On Arch Linux:

```sh
sudo pacman -S cmake qt6-base qt6-tools qtkeychain-qt6 procps-ng
```

Build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

Run the tests:

```sh
ctest --test-dir build --output-on-failure
```

Tests are grouped by CTest labels — `unit`, `integration`, `database`, `domain`, `process`,
`application`, `core`, `resources` — so a subset can be run with `ctest --test-dir build -L database`.

To build with coverage instrumentation, configure with `-DENABLE_COVERAGE=ON`. To generate install
rules, configure with `-DGAMELOG_ENABLE_INSTALL=ON`.

## Running

`gamelog` takes exactly one mode argument:

```sh
./build/src/gamelog --headless   # tracking only, no window
./build/src/gamelog --gui        # full interface: library, calendar, sessions
./build/src/gamelog --live       # compact window for the current session
```

A lock file allows only one runtime at a time, since the runtime owns the database and process
tracking. The database location can be overridden with `GAMELOG_DATABASE_PATH`.

Logging uses Qt logging categories under the `gamelog.*` namespace, so output can be filtered:

```sh
QT_LOGGING_RULES='gamelog.core.process.debug=true' ./build/src/gamelog --headless
```

## Steam setup

Steam synchronization needs a [Steam Web API key](https://steamcommunity.com/dev/apikey) and your
64-bit Steam ID. Enter both from the GUI (File menu); they are stored in the system keychain.

## Repository layout

- `src/core` — domain types, SQLite repositories, migrations, process detection, logging categories
- `src/application` — `GameLogRuntime` composition root and the application services
- `src/gui` — Qt Widgets interface
- `tests` — Qt Test suites and shared fixtures
- `resources` — images and SQL migrations, compiled in as Qt resources
- `docs` — design document and behavioral contracts
- `packaging` — desktop entry, systemd user service, launcher script

## Documentation

- [docs/GameLog_Software_Design_Document.md](docs/GameLog_Software_Design_Document.md) — the
  authoritative design document, kept aligned with the implementation
- [docs/CONTRACT_CHANGES.md](docs/CONTRACT_CHANGES.md) — the behavioral contracts the test suite enforces
- [docs/architecture.md](docs/architecture.md) — short orientation summary

## Roadmap

- [x] Session note-taking
- [x] Automatic session pickup/dropoff, marking cut-off sessions as interrupted
- [x] Scan the Steam library for games *(works; needs refinement)*
- [ ] Per-game summary screen
- [ ] Finish the artwork workflow, including custom artwork
- [ ] Stylesheets and general GUI polish
- [ ] Persistent settings (poll interval and grace periods are currently compile-time constants)

## License

See [LICENSE](LICENSE).
