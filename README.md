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

Here is Claude's plan:

# Test Suite Expansion — GameLog

## Context

We recently completed a contract-verification pass over this codebase, producing `CONTRACT_CHANGES.md` (36 behavioral
contracts) and correcting `GameLog_Software_Design_Document.md`. That pass changed production code (`SessionRepository`
timestamp normalization, `CalendarView` month range) and rewrote contracts 18/19/25/29 around what the schema actually
permits.

The test suite has not kept up. It covers 5 of 21 functional translation units, contains **zero test doubles**, and
**does not currently compile**. This plan brings the suite up to the contract document *and* to a baseline of per-method
functional coverage.

Three blockers must be fixed before any new tests are added:

1. **The build is broken.** Test files were recently moved from `tests/application/services/` to
   `tests/application/services/local/` to mirror `src/`, but three `#include "../../../src/..."` relative paths were not
   updated for the new depth. `ctest` currently reports "5 tests passed" from *stale binaries* — it is not a real pass.
2. **A latent dangling reference** exists in both service tests: `credService` and `steamService` are stack locals in
   `init()` that are destroyed on return, while `GameService` stores a `SteamApiService&`. Tests pass today only because
   none of them trigger a Steam call.
3. **`tests/fixtures/CMakeLists.txt`** contains `add_subdirectory(fixtures)` — self-recursive and never evaluated.
   `tests/CoreSmokeTest.cpp` is registered nowhere and never compiles.

## Decisions taken

- **Contract item 4**: extract `determineRunMode` out of `main.cpp`'s anonymous namespace into a testable unit.
- **CredentialService**: cover validation + null-job paths only. `QKeychain::Job::start()` is not virtual (verified in
  `/usr/include/qt6keychain/keychain.h:103`), so a fake job returned from the existing seam would still contact the real
  keychain. No further production seam.
- **Delivery**: one complete pass, built and verified.

---

## Coverage model

Every test file is built from **three layers**, not just the contract layer:

1. **Baseline** — each public method gets at least one test proving its normal, documented behavior: expected return
   value, expected state change, expected signal. This applies even to trivial methods (`artworkTypeToString`,
   `hasTrackedSteamGames`, the `AppPaths` accessors).
2. **Edge / boundary** — empty collections, zero and negative IDs, single-element results, `std::nullopt` optionals,
   blank and whitespace-only strings, missing rows, boundary timestamps, limit/offset at 0 and beyond the result size.
3. **Contract** — the specific behavior named in `CONTRACT_CHANGES.md`.

Tests are written to fail if the behavior breaks — not merely to execute lines. Where a method's behavior cannot be
observed without asserting private implementation detail, it is reported as a gap rather than given a fragile test.

### Method-level gaps in the *existing* tests, to be closed

| Class             | Public methods with no current test                                                                                                                          |
|-------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `GameService`     | `findByExecutablePath`, `trackedSteamGames`, `trackedPathGames`, `hasTrackedSteamGames`, `setHasArtwork`, `syncSteamGames`                                   |
| `SessionService`  | `search`, `getSessionsInTimeRange`, `startAutomaticSession`, `endActiveSession`, `restoreActiveSession`, `updateAutomaticTracking`, `resetAutomaticTracking` |
| `DatabaseManager` | repeated-`initialize()` idempotency, blank/whitespace path rejection, explicit `:memory:`                                                                    |

---

## Phase 0 — Repair

**Fix broken includes** in `tests/application/services/local/GameServiceTest.cpp` and `SessionServiceTest.cpp`. Replace
`#include "../../../src/application/services/local/GameService.h"` with the canonical target-provided form already used
by production code:

```cpp
#include "application/services/local/GameService.h"
```

`gamelog-application` exports `src/` and `src/core` PUBLIC, so this resolves regardless of test-file depth. Apply the
same to `SessionService.h`.

**Fix the dangling reference** in both `init()` methods — promote to members so lifetime matches the service that
references them:

```cpp
std::unique_ptr<gamelog::application::services::CredentialService> credentialService_;
std::unique_ptr<gamelog::application::services::SteamApiService>   steamApiService_;
```

Construct before `GameService`, reset after it in `cleanup()`.

**Delete** `tests/fixtures/CMakeLists.txt` (dead and self-recursive). **Register** `tests/CoreSmokeTest.cpp`.

---

## Phase 1 — Production change (contract item 4)

New `src/application/RunMode.h` / `RunMode.cpp`, added to `src/application/CMakeLists.txt`:

```cpp
namespace gamelog::application
{
    enum class RunMode { Headless, Gui, Live };

    [[nodiscard]] std::optional<RunMode> determineRunMode(int argc, char* argv[]);
}
```

Move the existing body verbatim from `src/main.cpp:35-43` — no behavior change. `main.cpp` includes the header and
deletes its local copy; its `RunMode` references become `gamelog::application::RunMode`.

---

## Phase 2 — Fixtures

Reuse `tests/fixtures/TestDatabaseFixture.h` everywhere a database is needed. Two additions, both in `tests/fixtures/`:

**`FakeNetworkAccessManager.h/.cpp`** — justified: two separate test files (`SteamApiServiceTest`,
`GameArtworkServiceTest`) need to drive `QNetworkAccessManager` without network, and there is no existing seam.
`createRequest()` is `virtual` and `protected` (verified at
`/usr/include/qt6/QtNetwork/qnetworkaccessmanager.h:146-147`); `QNetworkReply` needs only `abort()` overridden plus
`readData()`, and exposes protected `setError`/`setFinished`/`setAttribute`/`setUrl`.

Two critical details:

- The fake reply must emit `finished()` **asynchronously** (`QTimer::singleShot(0, ...)`), because `SteamApiService`
  connects to `reply->finished` *after* `get()` returns.
- `GameArtworkService` connects to the **manager-level** `QNetworkAccessManager::finished(QNetworkReply*)`. A subclass
  overriding `createRequest` bypasses QNAM's internal machinery, so that signal is **not** emitted automatically — the
  fake must emit it itself when a queued reply completes.

API: register canned responses keyed by URL substring (`"library_600x900.jpg"`, `"header.jpg"`, `"logo.png"`), each
carrying HTTP status, `QNetworkReply::NetworkError`, and body bytes; plus a recorded list of requested URLs so tests can
assert on query parameters (contract item 7).

**`FakeProcessSource.h`** — header-only; implements the existing pure-virtual seam
`core::process::ProcessSource::listProcesses()` by returning a caller-supplied `std::vector<ProcessInfo>`. Header-only
is sufficient (no `Q_OBJECT`, no state beyond the vector), so no `.cpp` is warranted.

Image bytes for artwork tests stay a **local helper** in `GameArtworkServiceTest.cpp` (`QImage` + `QBuffer` → real
decodable JPEG/PNG), not a fixture — only one file needs them.

---

## Phase 3 — New test files

Mirror `src/` layout. Follow existing conventions exactly: anonymous-namespace `class XTest : public QObject` with
`Q_OBJECT`; helpers in a preceding anonymous namespace; `init()`/`cleanup()` only (no `initTestCase`); naming
`methodUnderTest_expectedBehavior`; `QVERIFY`/`QCOMPARE`; `QTEST_APPLESS_MAIN` for pure unit tests and
`QTEST_GUILESS_MAIN` for database/service tests; trailing `#include "<FileBaseName>.moc"`; fixture calls fully qualified
as `gamelog::tests::fixtures::...`; member fields with trailing underscore.

`QSignalSpy` is introduced where signals are the contract (items 16, 21, 22). It is the only Qt facility for this and
has no existing alternative in the suite.

### `tests/core/database/GameRepositoryTest.cpp` — database helper

- **Baseline**: `insert` assigns an id and persists every field; `query` round-trips a row; `update` modifies fields;
  `remove` deletes and returns true.
- **Edge**: query on empty table; `remove(0)`, `remove(-1)`, missing id → false; `update` on missing row → false;
  `insert` with unset `steamAppId`/`executablePath`; limit 0, offset past end, limit+offset combined; sort by Title vs
  Id, Ascending vs Descending; `title COLLATE NOCASE` matching.
- **Contract**: 10 (insert requires `id == 0`; update requires positive id; blank/whitespace title rejected;
  non-positive Steam App ID rejected; `hasArtwork` non-null default false), 11 (duplicate Steam App ID insert **fails**
  rather than merging; duplicate executable name/path both permitted).

### `tests/core/database/SessionRepositoryTest.cpp` — database helper. *Highest value.*

- **Baseline**: `insert` assigns id and creates the document row; `query` round-trips; `update` modifies; `remove`
  deletes.
- **Edge**: `remove(0)`/negative/missing → false; empty result sets; every `SessionQuery` filter exercised at least once
  (`ids`, `gameIds`, `sources`, `statuses`, `minimum`/`maximumTrackedDuration`, `hasEndTimestamp`, sort field/direction,
  limit/offset).
- **Contract**: **18 — including the new `strftime` fix**: seed rows in *both* persisted formats (`...T12:00:00.000Z`
  via the normal path, `...T12:00:00Z` via raw SQL) and assert the half-open `[start, end)` boundary is correct for
  both, which is exactly the bug that change fixed. 20 (valid start; nonnegative duration; active has no end;
  completed/interrupted need valid end ≥ start; equal timestamps = valid zero-length). 26 (left join; missing document →
  empty notes, row still returned). 27 (document row always created; timestamp unchanged on unrelated update; strictly
  later on changed note). 28 (transactional; rollback restores `Session.id` to 0). 29 (corrupt row skipped, siblings
  still returned) — using `PRAGMA ignore_check_constraints = ON` per `CONTRACT_CHANGES.md` §"Corrupted persistence test
  setup" for the invalid-enum and negative-duration cases; invalid timestamp text and invalid status/end combinations
  insert directly.

### `tests/core/database/DatabaseMigratorTest.cpp` — database helper

- **Baseline**: `applyPendingMigrations()` on a fresh database returns true and records all four migrations with the
  expected versions and names; the expected tables and the `one_active_session` index exist afterward.
- **Edge**: applying twice is idempotent; running against a closed/invalid database returns false.
- **Contract**: 31 (unknown version `999` → false; correct version with wrong name → false), 32 (legacy artwork
  mapping).
- `knownMigrations()` is private, so everything is driven through public `applyPendingMigrations()`. For item 32, stage
  a database at version 3 by executing `:/migrations/001..003` from the Qt resources and seeding the ledger, insert
  `games` rows with `artwork_path` values (`NULL`, `""`, `"   "`, tab/newline mix, a real path), run
  `applyPendingMigrations()` so only 004 applies, and assert the `has_artwork` mapping. Reusing the real resource files
  avoids duplicating schema.

### `tests/core/process/ProcessHelpersTest.cpp` — plain helper

- **Baseline**: `processMatchesGame` true on matching Steam App ID; true on matching executable path when neither side
  has Steam identity; `matchTrackedGame` returns the Steam entry, then the path entry, then `nullptr`.
- **Edge**: `readProcessEnvironmentValue` with pid ≤ 0 or empty variable name → `nullopt`; `readSteamAppId` on a
  nonexistent pid → `nullopt`; empty tracked hashes → `nullptr`; empty executable path skips path matching; game with
  `steamAppId` set but ≤ 0.
- **Contract**: 33 (when both sides have Steam App IDs the IDs are authoritative — a mismatch stays false **even when
  executable paths are identical**).

### `tests/core/process/SteamProcessInspectorTest.cpp` — plain helper

- **Baseline**: `annotate` populates `steamAppId` from the injected reader; the default constructor is exercised for
  construction only (no `/proc` dependence asserted).
- **Edge**: empty snapshot; reader returning `nullopt`; a null/empty `SteamAppIdReader`.
- **Contract**: 34 (reader invoked once per new pid and **not** re-invoked while the pid/executable-path entry stays
  live; re-read when the executable path changes for the same pid; cache entries for absent pids purged — verified by
  observing reader invocation counts through the injected lambda, which is a public seam, not private state).

### `tests/core/resources/AppPathsTest.cpp` — plain helper

- **Baseline + edge** for all four statics, asserting *relative structure only*, never absolute developer paths:
  `dataDirectory()` non-empty; `databasePath()` sits under `dataDirectory()` and ends in `gamelog.sqlite`;
  `artworkDirectory()` under `dataDirectory()`; `gameArtworkDirectory(id)` equals `artworkDirectory()/<id>`; distinct
  ids give distinct directories; behavior for id `0` and negative ids documented as-is. Uses
  `QStandardPaths::setTestModeEnabled(true)`.

### `tests/application/RunModeTest.cpp` — application helper

- **Contract 4 in full**, which is also the baseline: each of `--headless`, `--gui`, `--live` maps to its enum; no
  arguments, two run-modes, a repeated run-mode, a valid mode plus an extra argument, an unknown argument, and an
  empty-string argument all yield `nullopt`. Plus defensive `argc`/`argv` edges (`argv == nullptr`,
  `argv[1] == nullptr`).

### `tests/application/GameLogRuntimeTest.cpp` — application helper

- **Baseline**: construction against a fresh database exposes non-null `getGameService`/`getSessionService`/
  `getArtworkService`/`getCredentialService`; `start()` succeeds; `update()` polls the injected `FakeProcessSource`;
  `stop()` completes.
- **Edge**: `update()` before `start()` and after `stop()` is a no-op; `update()` with zero/negative elapsed is a no-op;
  a factory returning `nullptr` makes `start()` fail; accessors return `nullptr` when database init failed (blank path).
- **Contract**: 35 (`start()` while running fails; `start() → stop() → start()` works on the same instance, recreating
  the process source and restoring state).

### `tests/application/services/local/CredentialServiceTest.cpp` — application helper

- **Baseline + contract 5**: `setSecret`, `getSecret`, `removeSecret` each reject empty and whitespace-only keys via
  `credentialError`; `setSecret` rejects empty and whitespace-only secrets and does **not** remove an existing value; a
  test subclass overriding the three `createXPasswordJob()` seams to return `nullptr` drives the "unable to create job"
  error path for all three. `QSignalSpy` verifies exactly one `credentialError` and no `secretStored`/`secretRemoved`.
  No keychain contact.

### `tests/application/services/web/SteamApiServiceTest.cpp` — application helper

- No keychain needed: `Q_SIGNALS` expands to `public` (verified at `/usr/include/qt6/QtCore/qtmetamacros.h:48`), so the
  test constructs a real `CredentialService` (trivial constructor) and emits `secretRetrieved` / `secretNotFound` /
  `credentialError` on it directly to drive the state machine, paired with `FakeNetworkAccessManager`.
- **Baseline**: `getOwnedGames()` requests both credentials; once both arrive, one HTTP request is issued and a
  well-formed response emits `ownedGamesReceived` with the right game count.
- **Edge**: a second `getOwnedGames()` while one is in flight fails; duplicate `secretRetrieved` does not start a second
  request; credentials arriving in either order both work; unrelated credential keys are ignored; malformed JSON, HTTP
  error status, and a non-numeric or zero player ID each fail.
- **Contract**: 6 (blank key or player id fails immediately, clears state, emits a credential-specific error, and does
  not hang), 7 (API key appears **only** in the `key` query parameter, no `x-webapi-key` header — asserted against the
  fake's recorded request), 8 (`{"response":{"games":[]}}` succeeds as an empty library; `{"response":{}}`, non-object
  root, non-object `response`, and non-array `games` each emit `requestFailed`).

### `tests/application/services/web/GameArtworkServiceTest.cpp` — application helper

- `QStandardPaths::setTestModeEnabled(true)` in `init()` so `AppPaths::artworkDirectory()` redirects away from the
  developer's real data directory; remove the tree in `cleanup()`.
- **Baseline**: `artworkTypeToString` for all three enum values; `makeGameArtworkDirectory` creates the directory and
  returns true; `getGameArtwork` returns true and emits `artworkAvailable(Cover)` when a valid local `cover.jpg` exists.
- **Edge**: `makeGameArtworkDirectory(0)` and negative → false; `getGameArtwork` with `id <= 0` → false; game with no
  Steam App ID queues nothing; empty cover file; directory present but no cover.
- **Contract**: 14 (queueing downloads alone returns false), 15 (only a nonempty decodable `cover.jpg` counts; retried
  on later calls), 16 (signals carry `(gameId, ArtworkType)`; header/logo success emits **no** `Cover` signal), 17 (a
  JPEG-labelled payload that is actually PNG fails validation and leaves no file on disk).

---

## Phase 4 — Modified test files

- **`tests/core/domain/SessionTest.cpp`** — baseline already covers the accepted spellings. Add the `operator<<` smoke
  checks for `SessionSource`, `SessionStatus`, and `Session`, and contract 9: empty, whitespace-padded, uppercase, and
  unknown strings must throw `std::invalid_argument` for both parse functions. Match the existing file-scope
  `const std::vector<QString>` data-vector style rather than introducing `_data()` methods.

- **`tests/core/database/DatabaseManagerTest.cpp`** — close the item-30 gaps listed above: repeated `initialize()`
  returns true idempotently; blank and whitespace-only paths fail; explicit `:memory:` initializes; a failed initialize
  leaves no half-open state and does not remove a colliding connection owned by someone else. Keep the existing
  `QCOMPARE(query.value(0).toInt(), 4)` migration-count assertion in sync.

- **`tests/application/services/local/GameServiceTest.cpp`** — add baselines for the six untested methods
  (`findByExecutablePath` hit and miss, `trackedSteamGames`/`trackedPathGames` contents after sync,
  `hasTrackedSteamGames` true and false, `setHasArtwork` set/clear/no-op-when-unchanged/false-for-missing-game,
  `syncSteamGames` delegating to the Steam service). Then contract 12 (sync searches the whole database and leaves
  existing rows — including untracked ones and local titles — completely unchanged; malformed entries and non-positive
  app ids skipped) and 13 (index rebuild; duplicate executable path retains `QHash` replacement). Note
  `QTest::failOnWarning()` is armed in `init()`, so any test provoking a warning must pair
  `QTest::ignoreMessage(QtWarningMsg, QRegularExpression(...))`.

- **`tests/application/services/local/SessionServiceTest.cpp`** — the largest expansion. Baselines for the seven
  untested methods first (`search` with a populated query, `getSessionsInTimeRange` normal window,
  `startAutomaticSession` happy path, `endActiveSession` happy path, `restoreActiveSession` with zero and with one
  active row, `updateAutomaticTracking` across the start grace period and the end grace period, `resetAutomaticTracking`
  clearing pending state). Then contracts 18 (half-open window at the service level), 19 (service-layer rejection of a
  second active row from add/update/start), 21 + 22 (`QSignalSpy` on `sessionStarted`/`sessionStopped`, including **no**
  emission for active→active and inactive→inactive, rejection of active-row removal, and a slot mutating its by-value
  `Session` copy then persisting it), 23 (duration replaced by wall-clock rather than added; invalid or
  earlier-than-start clock fails without touching persistence), 24 (pending-game retention, Steam-over-path priority,
  lower-id tie-break, `trackingEnabled == false` rejection), 25 (orphaned active row via `PRAGMA foreign_keys = OFF`).
  Use the **3-arg `Clock`-injecting constructor**, currently never exercised, to make items 23 and 24 deterministic.

Per the revised contract item 19, the **multi-active repair path is explicitly not covered** and the
`one_active_session` index must not be dropped.

---

## Phase 5 — CMake

In `tests/CMakeLists.txt`:

1. Add `Qt6::Network` to `gamelog-test-support` PUBLIC links (needed by `FakeNetworkAccessManager`) and add the new
   fixture sources to that target.
2. Add a third helper for application-layer tests that need `gamelog-application` but are not database/integration
   tests, and refactor the existing database helper to delegate to it so current behavior is preserved exactly:

```cmake
function(add_gamelog_application_test target_name source_file labels)
    add_gamelog_test(${target_name} ${source_file} "${labels}")

    target_link_libraries(${target_name}
            PRIVATE
            gamelog-test-support
            gamelog-application
            Qt6::Sql
    )
endfunction()

function(add_gamelog_database_test target_name source_file)
    add_gamelog_application_test(${target_name} ${source_file} "database;integration")
    add_dependencies(gamelog-database-tests-build ${target_name})
endfunction()
```

`Qt6::Network`, `Qt6::Gui`, and `Qt6Keychain` arrive transitively via `gamelog-application`'s PUBLIC links — no explicit
naming needed.

3. Register every new test under the existing comment banners, kebab-case `gamelog-<thing>-test`, plus
   `CoreSmokeTest.cpp`.

---

## Verification

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc) 2>&1 | grep -E "error:|warning:"
cd build && ctest --output-on-failure
```

Success = zero compiler warnings (the tree currently builds clean under
`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion`) and every test passing. I will report the real command
output, and will state explicitly if anything fails rather than summarizing around it.

I will also re-run the suite once with `ctest --repeat until-fail:2` to catch order- or state-dependent flakiness
introduced by the shared temp-database directory.

---

## Intentionally untested

Per contract item 1, translation units whose only behavior is a `QDebug operator<<` or logging-category definition need
no dedicated test: `Game.cpp`, `SessionDocument.cpp`, `QueryOptions.cpp`, `GameQuery.cpp`, `SessionQuery.cpp`,
`LoggingCategories.cpp`. (`Session.cpp`'s `operator<<` gets a light smoke check anyway, since that file is already in
scope for its parse functions.)

`src/gui/**` is out of scope per your instruction.

`ProcfsProcessSource.cpp` keeps its existing live-system smoke test (`ProcessTest.cpp`) per item 3 — asserting only that
enumeration succeeds, never that a particular process is present. Its `listProcesses()` is additionally exercised
indirectly wherever a real snapshot is not required.

## Known gaps to report at the end

- **CredentialService success paths** (`secretStored`, `secretRetrieved`, `secretNotFound`) remain uncovered —
  `QKeychain::Job::start()` is non-virtual, so reaching them requires either a real keychain or a new production seam.
  Contract item 5 (validation) is fully covered.
- **Contract item 19's multi-active repair** and **item 29's schema-blocked classes** are unreachable-by-design; the
  former is deliberately uncovered, the latter reachable only via `PRAGMA ignore_check_constraints`.
- `ProcessHelpers::readProcessEnvironmentValue` / `readSteamAppId` can only be tested at their failure edges without
  depending on a live process's `/proc/<pid>/environ`; their success paths are covered indirectly through the injected
  `SteamAppIdReader` seam.

