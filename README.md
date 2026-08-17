# GameLog

GameLog is a Linux-first desktop application scaffold for locally tracking PC gaming sessions.

## Current status

dd This repository currently contains the initial project scaffold only. Most planned features are represented by stubs
with TODOs and are intentionally unimplemented.

## Planned architecture

- `gamelog-core` static library shared by all executables.
- `gamelog` Qt Core background executable.
    - Can run in `--headless` or `--gui` modes.

See `docs/architecture.md` for a short overview.

## Dependencies

- C++23 compiler (GCC or Clang)
- CMake
- Qt 6 development packages (Core, Widgets, Sql, Network, Test)
- libprocs
  2 https://gitlab.alpinelinux.org/alpine/aports/-/tree/master/main/procps-ng?__goaway_challenge=cookie&__goaway_id=574b5f7ee2299d04a0b9fa1214bc2c1f&__goaway_referer=https%3A%2F%2Fpkgs.alpinelinux.org%2F

Example Arch Linux package names:

- `cmake`gamegamegamegamegameecific gam
- `qt6-base`
- `qt6-tools` (if your environment separates Qt tooling)
- `libprocs`

## Repository layout

- `src/core`: shared domain and service stubs
- `src/application`: background agent executable
- `src/gui`: Qt Widgets location
- `tests`: starter Qt Test smoke test
- `resources`: placeholder icons and migration directories
- `packaging`: placeholder packaging/service files

## TODO:

- Add note-taking functionality. DONE
- Add automatic session pickup/dropoff, label cut off sessions as interrupted.
- Add Summary screen for specific game.
- Scan installed steam library for games. DONE, but needs editing.
- Figure out artwork situation.
- Stylesheets 
