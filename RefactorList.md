# GameLog — Code Quality & Architecture Review

**Status: proposal only. Nothing is approved. Reply with the IDs you want (e.g. `S1, S2, S4, M5, M6`).**

---

## Context

You asked for a broad code-quality and architectural review of GameLog before adding features, as a refactoring/cleanup
pass rather than a rewrite, with backend correctness and testability prioritized over the incomplete GUI. This document
is the result of reading every `.cpp`/`.h` under `src/`, the migrations, the CMake configuration,
`docs/CONTRACT_CHANGES.md`, and the last recorded CTest run.

The short version: **the backend architecture is sound and should not be redesigned.** The layering you built (GUI →
services → repositories → SQLite) holds up under tracing, the query-specification pattern is genuinely good, and the
contract document is better than most professional codebases have. What the code needs is a consistency and hygiene
pass, four or five targeted structural fixes, and a handful of documentation repairs — not an architectural change. **No
Large refactor is recommended.**

---

## 1. Current Architecture Summary

**Entry point.** [main.cpp](src/main.cpp) parses exactly one run-mode flag via
[determineRunMode](src/application/RunMode.cpp#L7), picks `QCoreApplication` vs `QApplication`, takes a
`QLockFile` in `$XDG_RUNTIME_DIR/gamelog/runtime.lock` to enforce one runtime per machine, resolves the database path
(CLI → `GAMELOG_DATABASE_PATH` → `QStandardPaths`), constructs `GameLogRuntime`, starts it, then optionally creates
`MainWindow` or `LiveWindow`. A 5-second `QTimer` drives `runtime.update(5s)`.

**Composition.** [GameLogRuntime](src/application/GameLogRuntime.h) owns everything by value or
`std::optional`, in dependency order: `DatabaseManager` → `GameRepository`/`SessionRepository` →
`CredentialService` → `SteamApiService` → `GameService` → `SessionService` → `GameArtworkService`. The
`std::optional` members express "constructed only if the database initialized", and the `get*Service()`
accessors return `nullptr` in that failure case. Ownership is unambiguous and there is no `new` in the runtime. The
process source is recreated per `start()` from a factory, which is what makes `start → stop → start` work.

**Process tracking flow.** `update()` → `ProcfsProcessSource::listProcesses()` (libproc2, `PIDS_ID_PID`,
`PIDS_CMD`, `PIDS_EXE`) → if any tracked Steam games exist, `SteamProcessInspector::annotate()` fills
`ProcessInfo::steamAppId` from a PID-keyed cache that only re-reads `/proc/<pid>/environ` on a new PID or a changed
executable path → `SessionService::updateAutomaticTracking(processes, elapsed)`. That method runs a two-sided
grace-period state machine: 30s of continuous detection starts a session, 30s of continuous absence ends one. Candidate
selection prefers a still-detected pending game, then Steam identity, then path identity, then lowest game ID.

**Database flow.** `DatabaseManager` owns one named Qt connection (`GameLogRuntimeConnection`), sets
`foreign_keys = ON` and `busy_timeout = 5000`, then runs `DatabaseMigrator`, which validates the
`schema_migrations` ledger against a compiled-in list and applies each pending `.sql` resource inside a transaction,
splitting on `-- statement-break`. Four migrations exist. Repositories translate `GameQuery` /
`SessionQuery` structs into parameterized SQL; no SQL escapes the repository layer. `SessionRepository` joins
`session_documents` for notes and keeps the session row and its document row in one transaction.

**Signal/slot flow.** `CredentialService::secretRetrieved/secretNotFound/credentialError` →
`SteamApiService`; `SteamApiService::ownedGamesReceived` → `GameService::onSteamGamesReceived`;
`GameService::gameAdded` → `GameArtworkService::getGameArtwork`; `GameArtworkService::artworkAvailable/
artworkUnavailable` → a runtime lambda calling `GameService::setHasArtwork`;
`SessionService::sessionStarted/sessionStopped` → `MainWindow` and `LiveWindow`. All connections are direct
(single-threaded).

**External integrations.** Steam Web API `GetOwnedGames` (key only ever in the query string, never a header, never
logged); Steam CDN artwork (`library_600x900.jpg`, `header.jpg`, `logo.png`) validated through `QImage`
before being written under `AppPaths::gameArtworkDirectory(gameId)`; `qt6keychain` for credential storage behind three
`virtual` job-factory seams.

---

## 2. Things That Are Already Good — Leave These Alone

These are deliberate design decisions that survive scrutiny. I am not proposing changes to any of them.

- **The query-specification pattern.** `GameQuery`/`SessionQuery` as persistence-neutral structs, with repositories
  owning all SQL, is the right boundary and is applied consistently. Do not replace it.
- **`GameLogRuntime` ownership.** Value/`optional` members in dependency order, no raw `new`, explicit
  non-copyable/non-movable, factory-recreated process source. This is a textbook composition root at 182 lines. It does
  not need decomposing.
- **`DatabaseManager` lifecycle handling.** The "never remove another owner's connection" logic in
  `closeDatabase()` and the close-on-any-post-open-failure path in `initialize()` are subtle and correct.
- **`SessionRepository` timestamp normalization.** The `strftime()` expression with its comment explaining why raw TEXT
  comparison is unsafe (`'Z'` sorts above `'.'`) is exactly the kind of code that earns its comment.
- **Transactional session+document writes,** including restoring `session.id` to zero after a rollback.
- **The injected-seam approach to testability.** `ProcessSourceFactory`, `SteamAppIdReader`, `Clock`,
  `QNetworkAccessManager&`, and the `virtual` keychain job factories are all narrow, production-neutral seams. This is
  much better than introducing interfaces everywhere, and it should stay.
- **`SteamProcessInspector`'s cache.** Read-once-per-PID with executable-path invalidation and purge-on-absence is the
  right shape and avoids re-reading `/proc` for every process every 5 seconds.
- **`ProcessHelpers::matchTrackedGame`'s documented pointer-lifetime contract.**
- **The migration system.** Version+name ledger validation, rejecting unknown/newer versions, resource-compiled SQL,
  transactional application.
- **`docs/CONTRACT_CHANGES.md`.** 36 numbered behavioral contracts that distinguish schema-enforced invariants from
  service-layer checks, and state which paths tests need not cover and why. Keep this current.
- **The test suite's structure.** `add_gamelog_test` / `..._application_test` / `..._database_test` layering, CTest
  labels, and the fixtures (`FakeProcessSource`, `FakeNetworkAccessManager`, `TestDatabaseFixture`).
- **`Session`'s invariant documentation** and the repository being the final enforcement boundary for it.

---

## 3. Refactoring Candidates

### Quick index

| ID  | Category | Title                                                                       | Recommendation |
|-----|----------|-----------------------------------------------------------------------------|----------------|
| S1  | Small    | Remove global `using` declarations from repository headers                  | **Do now**     |
| S2  | Small    | Route logging through the correct declared categories                       | **Do now**     |
| S3  | Small    | Remove hot-path and trivial-accessor logging noise                          | **Do now**     |
| S4  | Small    | Header/include hygiene (`#pragma once`, relative includes, dead includes)   | **Do now**     |
| S5  | Small    | `Q_SIGNALS`/`Q_SLOTS` so clang-format stops mangling access specifiers      | **Do now**     |
| S6  | Small    | Fix documentation typos and misplaced Doxygen                               | **Do now**     |
| S7  | Small    | Replace `std::pmr::map` and rename `ArtWorkTypeToSteamUrl`                  | **Do now**     |
| S8  | Small    | De-duplicate the two constructors in `GameArtworkService`/`SteamApiService` | **Do now**     |
| S9  | Small    | GUI namespace and member-naming consistency                                 | Nice cleanup   |
| S10 | Small    | `CalendarView`: stop emitting another object's signal                       | **Do now**     |
| S11 | Small    | CMake: remove duplicated block, apply warnings to all targets               | **Do now**     |
| S12 | Small    | Consistent ID types across query/repository/service                         | Nice cleanup   |
| S13 | Small    | Replace `validateGame`'s boolean flag parameter                             | Nice cleanup   |
| M1  | Medium   | Extract shared SQL predicate/paging builder                                 | Nice cleanup   |
| M2  | Medium   | Consolidate `SessionSource`/`SessionStatus` ↔ string conversions            | **Do now**     |
| M3  | Medium   | Extract the automatic-tracking state machine from `SessionService`          | **Do now**     |
| M4  | Medium   | Unify process→game matching precedence in one place                         | **Do now**     |
| M5  | Medium   | Fix quadratic Steam library synchronization                                 | **Do now**     |
| M6  | Medium   | Simplify `DatabaseMigrator` ledger checking (N+1 queries)                   | Nice cleanup   |
| M7  | Medium   | Move artwork-fetch policy out of `GameCard`                                 | Defer          |
| M8  | Medium   | Extract Steam JSON → `Game` mapping from `GameService`                      | Nice cleanup   |
| M9  | Medium   | Collapse `MainWindow`'s two duplicated credential dialogs                   | Defer          |
| —   | Large    | None recommended — see §4                                                   | —              |

---

### Small Refactors

---

**ID:** S1 **Category:** SMALL

**Problem:** Global-scope `using` declarations in public headers leak into every translation unit that includes them,
three layers deep.

**Location:** [GameRepository.h:11-13](src/core/database/GameRepository.h#L11-L13),
[SessionRepository.h:10-12](src/core/database/SessionRepository.h#L10-L12),
[CalendarView.h:13-14](src/gui/calendar/CalendarView.h#L13-L14)

**Evidence:** `using std::vector; using gamelog::core::domain::Game;` at namespace scope in
`GameRepository.h`. Because of it, `GameService.h` can write bare `Game` and `GameQuery`,
`LibraryView.h:59` can write `std::vector<Game>`, and `CalendarView.cpp:31` can write `vector<Session>` — a GUI file
relying on a `using` declared in a database header. `SessionService.h` correctly qualifies everything, which shows the
intended style.

**Proposed Change:** Delete the six `using` lines; qualify the declarations in the two repository headers; fix the
resulting unqualified names in `GameService.h`, `LibraryView.h`, and `CalendarView.cpp`. Add file-local
`using` inside `.cpp` files where it improves readability, matching `SessionService.cpp`.

**Why:** This is the single most-cited C++ header problem in your instructions, and here it has real reach — it is why a
calendar widget compiles against `std::vector` without including `<vector>`.

**What Stays:** Every type, every signature, all `.cpp`-local `using` declarations.

**Behavior Impact:** None **Risk:** Low (compile-time only; failures are loud)

**Files Likely Affected:** `GameRepository.h`, `SessionRepository.h`, `GameService.h`, `LibraryView.h`,
`CalendarView.h`, `CalendarView.cpp`

**Testing:** Fully protected — if it compiles and `ctest` passes, it is correct. No new tests. **Dependencies:** None.
Do this first; it makes S4 and S9 cleaner. **Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** S2 **Category:** SMALL

**Problem:** Four logging categories are declared, defined, and never used; one class logs under a category belonging to
a different class; two classes log with no category at all.

**Location:** [LoggingCategories.h](src/core/logging/LoggingCategories.h),
[CredentialService.cpp:17](src/application/services/local/CredentialService.cpp#L17),
[GameArtworkService.cpp](src/application/services/web/GameArtworkService.cpp) (11 bare `qWarning()`/`qInfo()`),
[ProcfsProcessSource.cpp:34,44](src/core/process/ProcfsProcessSource.cpp#L34),
[LiveWindow.cpp:88](src/gui/live_window/LiveWindow.cpp#L88),
`GameService.cpp` and `SessionService.cpp` (every call site)

**Evidence:** `gamelogGuiLog`, `gamelogProcessLog`, and `gamelogGameServiceLog` have zero call sites.
`gamelogSessionServiceLog` has exactly one — inside `CredentialService::setSecret`. `GameService` and
`SessionService` log everything under `gamelogRuntimeLog`. `GameArtworkService` uses uncategorized `qWarning()`.
`ProcfsProcessSource` uses `qCritical(gamelogCoreLog)` rather than the `qC*` form used everywhere else, and does not use
`gamelogProcessLog`.

**Proposed Change:** Point each class at its own category: `GameService` → `gamelogGameServiceLog`,
`SessionService` → `gamelogSessionServiceLog`, process code → `gamelogProcessLog`, GUI → `gamelogGuiLog`. Add
`gamelogArtworkLog` and `gamelogCredentialLog` and use them. Normalize `qCritical(cat)` → `qCCritical(cat)`.

**Why:** The category taxonomy already exists and encodes the architecture; it is simply unwired. Once wired,
`QT_LOGGING_RULES=gamelog.core.process.debug=true` becomes a real debugging tool.

**What Stays:** Every message text, every severity level, the category naming scheme.

**Behavior Impact:** Intentional — log output changes category names (and previously-uncategorized artwork messages
become filterable). No functional behavior changes. **Risk:** Low

**Files Likely Affected:** `LoggingCategories.h/.cpp`, `GameService.cpp`, `SessionService.cpp`,
`CredentialService.cpp`, `GameArtworkService.cpp`, `ProcfsProcessSource.cpp`, `LiveWindow.cpp`, `MainWindow.cpp`

**Testing:** `SteamApiServiceTest::getOwnedGames_neverLogsTheApiKeyOrTheQueryString` asserts on category filter rules —
verify it still behaves (note: it is *currently failing*, see B1). Tests using
`QTest::failOnWarning()` per contract item 36 must be re-checked. No new tests needed. **Dependencies:** None
**Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** S3 **Category:** SMALL

**Problem:** Trivial accessors log on every call, including inside the 5-second polling loop.

**Location:** [GameService.cpp:130-146](src/application/services/local/GameService.cpp#L130-L146),
[SessionService.cpp:41](src/application/services/local/SessionService.cpp#L41)

**Evidence:** `trackedSteamGames()`, `trackedPathGames()`, and `hasTrackedSteamGames()` each emit a `qCDebug`
line whose content is "Returning path tracked games". `hasTrackedSteamGames()` is called once per poll and
`trackedSteamGames()`/`trackedPathGames()` are called once *per process per poll* from `selectDetectedGame`. Separately,
`findActiveSession()` emits `qCWarning` when there is no active session — which is the normal resting state, and the GUI
calls it on every session start.

**Proposed Change:** Delete the debug lines from the three accessors. Downgrade `findActiveSession`'s warning to
`qCDebug` or remove it (the `std::nullopt` return already carries the information).

**Why:** These log statements restate the function name, and one of them turns "no session is running" — the common
case — into a warning. Under `-Wall` this also removes the only reason those `noexcept` accessors touch global state.

**What Stays:** All informational logging around real state transitions (session start/stop, sync counts).

**Behavior Impact:** Intentional (log volume only)
**Risk:** Low

**Files Likely Affected:** `GameService.cpp`, `SessionService.cpp`

**Testing:** Check for any test using `QTest::failOnWarning()` around `findActiveSession` in
`SessionServiceTest.cpp` before removing the warning. No new tests. **Dependencies:** Cleaner if done with S2.
**Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** S4 **Category:** SMALL

**Problem:** Header conventions are inconsistent across the tree, and several includes are wrong, missing, or dead.

**Location:** `AppPaths.h`, `GameArtworkService.h`, `MainWindow.h`, `LiveWindow.h`, `LibraryView.h`,
`GameCard.h`, `CalendarView.h`, `MainWindow.cpp`

**Evidence:**

- Seven headers use include guards plus a `// Created by kj on 8/14/26.` banner; the rest use `#pragma once`.
- [MainWindow.cpp:16](src/gui/main_window/MainWindow.cpp#L16) is
  `#include "../../application/services/local/SessionService.h"` — an escaping relative path, when the target already
  provides the include directory. Same pattern in `LibraryView.h:12` and `CalendarView.h:11-12`.
- `MainWindow.cpp` includes `<qloggingcategory.h>` (lowercase) and `logging/LoggingCategories.h` but logs nothing; it
  uses `QVBoxLayout` without including it.
- `MainWindow.h` forward-declares `QListWidget`, which is never used.
- [CalendarView.h:23](src/gui/calendar/CalendarView.h#L23) reads
  `QT_END_NAMESPACE class CalendarView : public QWidget` — the macro and the class declaration collapsed onto one line
  by clang-format.

**Proposed Change:** `#pragma once` everywhere; delete the generated banners; convert relative includes to the
target-relative form used by production code; add the missing `<QVBoxLayout>`; delete the dead includes and forward
declaration; put `QT_END_NAMESPACE` on its own line.

**Why:** Mechanical, zero-risk, and removes the "which convention does this file use?" question permanently.

**What Stays:** Every include that is actually needed; the `QT_BEGIN_NAMESPACE`/`Ui` forward-declaration idiom.

**Behavior Impact:** None **Risk:** Low

**Files Likely Affected:** ~8 headers, `MainWindow.cpp`

**Testing:** Compilation is the test. No new tests. **Dependencies:** Do after S1. **Recommendation:** Do now
**Approval:** AWAITING APPROVAL

---

**ID:** S5 **Category:** SMALL

**Problem:** clang-format does not recognize Qt's lowercase `signals`/`slots` keywords and mangles every access
specifier that uses them.

**Location:** Every `QObject` header — `GameService.h:136-137,143,177-178`, `SessionService.h:141`,
`SteamApiService.h:41,57-58`, `CredentialService.h:62`, `GameArtworkService.h:71`, `MainWindow.h:41,46-47`,
`LiveWindow.h:37-38`, `LibraryView.h:43-44`, `CalendarView.h:61-62`, `TextEditor.h:25-26,29-30`

**Evidence:** The formatter produces

```cpp
    public
        slots  :
```

and `signals  :` with two spaces before the colon, breaking the visual structure of every service header.

**Proposed Change:** Use the uppercase macros `Q_SIGNALS:` and `public Q_SLOTS:` / `private Q_SLOTS:`, which
clang-format formats correctly, and re-run `clang-format`. (Alternative: add `MacroBlockBegin`/statement-macro entries
to `.clang-format`, but the macro form is the standard Qt fix and is what your own test file already notes in a comment:
*"Q_SIGNALS expands to public"*.)

**Why:** Access specifiers are the primary navigation aid in a header, and right now every one of them in every service
is visually broken. Purely a formatting fix with no semantic change.

**What Stays:** Every signal, slot, and access level. `Q_SIGNALS`/`Q_SLOTS` are the same macros.

**Behavior Impact:** None **Risk:** Low

**Files Likely Affected:** ~12 headers

**Testing:** Compilation and `moc` generation. No new tests. **Dependencies:** Best done immediately before or after S4
so headers are touched once. **Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** S6 **Category:** SMALL

**Problem:** Doxygen typos, one comment attached to the wrong declaration, and inconsistent indentation.

**Location:** [Game.cpp:15](src/core/domain/Game.cpp#L15),
[CredentialService.h:30-34](src/application/services/local/CredentialService.h#L30-L34),
[QueryOptions.h:7-9](src/core/domain/query/QueryOptions.h#L7-L9),
[SessionQuery.h:40-104](src/core/domain/query/SessionQuery.h#L40), `LibraryView.h:38`, `CalendarView.h:32`

**Evidence:**

- `Game.cpp` prints the field label as `"hasArtwor: "`.
- `CredentialService`'s constructor carries `@brief The key used to store the Steam Player Name in the
  keychain.` followed by `@param parent` — the brief belongs to a constant that no longer exists there.
- `QueryOptions.h`'s first doc block is indented to column 1 inside an indented namespace.
- `SessionQuery.h` uses a different comment indentation than every other header, plus `Id's`, `status's`.
- `querey` appears in two GUI headers.

**Proposed Change:** Fix each. No new documentation added, no trivial getters documented.

**Why:** These are outright errors in generated documentation, not style preferences.

**What Stays:** All existing documentation content and the `@brief`-first house style.

**Behavior Impact:** None (except one corrected `QDebug` output string)
**Risk:** Low

**Files Likely Affected:** `Game.cpp`, `CredentialService.h`, `QueryOptions.h`, `SessionQuery.h`,
`LibraryView.h`, `CalendarView.h`

**Testing:** If any test asserts on `QDebug` output of `Game`, it will need the corrected label. **Dependencies:** None
**Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** S7 **Category:** SMALL

**Problem:** A polymorphic-memory-resource container is used for a compile-time constant lookup table, and its name
violates every naming convention in the project.

**Location:** [GameArtworkService.h:128](src/application/services/web/GameArtworkService.h#L128),
[GameArtworkService.cpp:280-283](src/application/services/web/GameArtworkService.cpp#L280-L283)

**Evidence:** `const static std::pmr::map<ArtworkType, QString> ArtWorkTypeToSteamUrl;` — `std::pmr::map`
carries a `polymorphic_allocator` and exists to let callers supply a memory resource. Nothing here supplies one. The
member is also PascalCase with no trailing underscore, spells "ArtWork" with a capital W, and is named
`...ToSteamUrl` while holding filenames, not URLs. Meanwhile the file already has two clean
`switch`-based mappers (`expectedImageFormat`, `artworkExtension`) in an anonymous namespace.

**Proposed Change:** Replace with a file-local `switch` function `steamArtworkFileName(ArtworkType)` in the existing
anonymous namespace, matching its two neighbours, plus a small `constexpr std::array` of the three
`ArtworkType` values for `getSteamArtwork`'s loop. Remove the static member from the header entirely.

**Why:** It removes a static-initialization-order dependency and an allocator abstraction that buys nothing, it takes an
implementation detail out of the public header, and it makes the file internally consistent — three mappings, three
identical `switch` functions, adjacent to each other.

**What Stays:** The Steam CDN filenames, the URL format string, `makeSteamArtworkUrl`'s signature, the fact that all
three artwork types are requested.

**Behavior Impact:** None **Risk:** Low

**Files Likely Affected:** `GameArtworkService.h`, `GameArtworkService.cpp`

**Testing:** `GameArtworkServiceTest.cpp` covers URL construction and the download path. Run it. **Dependencies:** None
**Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** S8 **Category:** SMALL

**Problem:** Two constructors in each of two services duplicate their entire bodies verbatim.

**Location:** [GameArtworkService.cpp:52-122](src/application/services/web/GameArtworkService.cpp#L52-L122),
[SteamApiService.cpp:19-39](src/application/services/web/SteamApiService.cpp#L19-L39)

**Evidence:** `GameArtworkService`'s two constructors contain the *same 30-line lambda*, character for character —
property extraction, validation, error branch, `deleteLater`. `SteamApiService`'s two constructors contain the same
three `connect` calls. Any change to reply handling must be made twice, in code where the compiler will not notice if
you only change one.

**Proposed Change:** In `GameArtworkService`, extract the lambda body into a private
`void onNetworkReplyFinished(QNetworkReply*)` and add a private `void connectNetworkAccessManager()` called by both
constructors. In `SteamApiService`, make the two-argument constructor delegate to the three-argument one (or extract
`connectCredentialService()`).

**Why:** Textbook accidental duplication with a real divergence hazard. Both constructors are documented as differing
*only* in manager ownership, so the shared body should be shared in code too.

**What Stays:** Both constructor signatures, the owned-vs-injected manager distinction, all reply-handling logic,
`deleteLater` placement.

**Behavior Impact:** None **Risk:** Low

**Files Likely Affected:** `GameArtworkService.h/.cpp`, `SteamApiService.h/.cpp`

**Testing:** `GameArtworkServiceTest.cpp` and `SteamApiServiceTest.cpp` both exercise the injected-manager constructor;
`GameLogRuntimeTest.cpp` exercises the owned-manager path. Both paths are covered. **Dependencies:** None
**Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** S9 **Category:** SMALL

**Problem:** Four of six GUI classes live in the global namespace while two live in `gamelog::gui`, and member naming is
inconsistent within the GUI layer.

**Location:** `LibraryView.h`, `GameCard.h`, `CalendarView.h`, `TextEditor.h` (global namespace);
`LiveWindow.h:58-61` (no trailing underscores); `MainWindow.h:42-44`

**Evidence:** `MainWindow` and `LiveWindow` are `gamelog::gui::`; `LibraryView`, `GameCard`, `CalendarView`, and
`TextEditor` are not. `MainWindow` uses `runtime_` but `ui`; `LiveWindow` uses `gameLogRuntime`,
`clockTimer`, `currentTime`, `ui` with no underscores at all, while every backend class uses a trailing underscore
consistently. The signal pair is `steamAPIKeyEntered` / `steamPlayerIdEntered` — `API` vs `Id`.

**Proposed Change:** Move the four classes into `namespace gamelog::gui`. Apply trailing underscores to
`LiveWindow`'s members. Rename `steamAPIKeyEntered` → `steamApiKeyEntered` (matching `SteamApiService`).

**Why:** The dominant convention in the repository is already clear; these four files simply predate it. It also
prevents future global-namespace collisions in a Qt Widgets app.

**What Stays:** Every class, every widget, all behavior, the `Ui::` forward-declaration idiom.

**Behavior Impact:** None **Risk:** Low — but it touches `.ui`-generated symbol lookup and every `connect` site, so
build all GUI targets.

**Files Likely Affected:** the four GUI header/source pairs plus `MainWindow.cpp`, `LiveWindow.cpp`

**Testing:** No GUI tests exist. Compilation plus a manual `--gui` and `--live` launch is the verification.
**Dependencies:** Do after S1 (the global `using` removal changes some of the same lines). **Recommendation:** Nice
cleanup **Approval:** AWAITING APPROVAL

---

**ID:** S10 **Category:** SMALL

**Problem:** A widget emits another object's signal to trigger its own slot.

**Location:** [CalendarView.cpp:20-21](src/gui/calendar/CalendarView.cpp#L20-L21)

**Evidence:**

```cpp
emit
calendar_->currentPageChanged(calendar_->yearShown(), calendar_->monthShown());
```

This compiles only because `emit` is an empty macro; it is a direct call to `QCalendarWidget`'s signal from outside that
class, purely to reach `CalendarView::onPageChanged`. The constructor also assigns
`gameService_`, `sessionService_`, and `calendar_` in the body rather than the initializer list, leaving
`calendar_` the only member without a default initializer.

**Proposed Change:** Call `onPageChanged(calendar_->yearShown(), calendar_->monthShown())` directly. Move the member
assignments into the initializer list.

**Why:** Emitting a foreign object's signal is a Qt anti-pattern that misleads anyone tracing the signal graph, and the
direct call is shorter and obviously equivalent.

**What Stays:** The initial-population behavior, the `currentPageChanged` connection, the highlighting logic.

**Behavior Impact:** Potential — any *other* slot connected to `QCalendarWidget::currentPageChanged` would no longer
fire during construction. Nothing else is connected today, so this is expected to be a no-op. **Risk:** Low

**Files Likely Affected:** `CalendarView.cpp`, `CalendarView.h`

**Testing:** No test covers `CalendarView`. Manual `--gui` verification that the calendar highlights sessions on open.
**Dependencies:** None **Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** S11 **Category:** SMALL

**Problem:** The compiler warning set is applied to only one of the four targets, and the root `CMakeLists.txt`
contains a duplicated configuration block.

**Location:** [CMakeLists.txt:38-54](CMakeLists.txt#L38-L54),
[src/application/CMakeLists.txt](src/application/CMakeLists.txt),
[src/gui/CMakeLists.txt](src/gui/CMakeLists.txt)

**Evidence:** Lines 38–44 (`include(CTest)`, `include(GNUInstallDirs)`, `find_package(Qt6 ...)`,
`pkg_check_modules(PROC2 ...)`) are repeated verbatim at lines 48–54, straddling the
`add_library(gamelog-warnings INTERFACE)` call. Separately, `gamelog-warnings` is linked by `gamelog-core`
(PRIVATE) and by the test targets — but **not** by `gamelog-application` or `gamelog-gui`. So
`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion` never runs over the services or the GUI. A concrete
consequence: `LibraryView.cpp:49` writes `for(auto i = 0; i < games.size(); ++i)`, an `int` vs `size_t`
comparison that `-Wextra` would flag.

**Proposed Change:** Delete the duplicated block. Add `gamelog-warnings` to `gamelog-application` and
`gamelog-gui` as PRIVATE. Fix whatever warnings that surfaces (expected to be small and localized — sign comparisons and
narrowing conversions in the GUI).

**Why:** You configured a strict warning set and then applied it to a quarter of the codebase. Enabling it before the
structural refactors means the compiler helps verify them.

**What Stays:** The warning flag list, the coverage option, the install rules, the target structure.

**Behavior Impact:** None expected — but fixing surfaced warnings may involve real casts, which I would report
individually rather than applying silently. **Risk:** Moderate — unknown warning count until it is switched on. I would
run it first and report the list before changing any code.

**Files Likely Affected:** `CMakeLists.txt`, `src/application/CMakeLists.txt`, `src/gui/CMakeLists.txt`, plus whatever
the warnings identify.

**Testing:** Full build and full `ctest`. **Dependencies:** None, but do it *early* so subsequent refactors are checked.
**Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** S12 **Category:** SMALL

**Problem:** Game IDs are `int`, `std::int64_t`, and `qlonglong` depending on which function you are in.

**Location:** `Game.h:21` (`int id`), `GameQuery.h:33` (`std::vector<std::int64_t> ids`),
`GameRepository.h:57` (`remove(std::int64_t)`), `GameService.h:65,100` (`findById(std::int64_t)`,
`removeGame(std::int64_t)`), `SessionQuery.h:43,48` (`std::vector<int>`), `SessionRepository.h:62`
(`remove(int)`)

**Evidence:** `GameRepository::insert` assigns `game.id = insertedId.toInt()` after validating the value as
`toLongLong()`. `GameService::setHasArtwork(int gameId, ...)` calls `findById(std::int64_t)`. `SessionQuery`
uses `int` for the same conceptual ID that `GameQuery` stores as `std::int64_t`.

**Proposed Change:** Standardize on `int` everywhere, matching the domain structs and `SessionQuery`, since that is
already the type the value is truncated to on the way out of the database. Keep the `qlonglong`
`QVariant` binding at the SQL boundary where Qt requires it.

**Why:** One conceptual type should have one C++ type. The current mix implies a 64-bit ID space that
`Game::id` cannot represent, which is a silent-truncation hazard rather than a style question.

**What Stays:** SQLite's `INTEGER PRIMARY KEY`, all query semantics, the `QVariant::fromValue<qlonglong>`
bindings.

**Behavior Impact:** None in practice (SQLite rowids beyond `INT_MAX` are unreachable here), but this is a
public-signature change. **Risk:** Low

**Files Likely Affected:** `GameQuery.h/.cpp`, `GameRepository.h/.cpp`, `GameService.h/.cpp`

**Testing:** `GameRepositoryTest.cpp` and `GameServiceTest.cpp` cover ID-based query, update, and removal including zero
and negative IDs. Well protected. **Dependencies:** None **Recommendation:** Nice cleanup **Approval:** AWAITING
APPROVAL

---

**ID:** S13 **Category:** SMALL

**Problem:** A validation helper takes a boolean flag and branches its error messages on it.

**Location:** [GameRepository.cpp:43-67](src/core/database/GameRepository.cpp#L43-L67); also `GameService.cpp:148`
parameter-name mismatch

**Evidence:** `validateGame(const Game& game, bool inserting)` starts with
`if((inserting && game.id != 0) || (!inserting && game.id <= 0))` followed by a ternary that picks one of two warning
strings. At the call sites this reads as `validateGame(game, true)` — the reader must look up what
`true` means. Separately, `GameService::setHasArtwork` declares its parameter `available` in the header and
`hasArtwork` in the definition.

**Proposed Change:** Split into `validateGameForInsert` and `validateGameForUpdate`, both delegating to a shared
`validateGameFields` for the title and Steam App ID rules. Align the `setHasArtwork` parameter name with the header.

**Why:** Removes a boolean-trap call site and makes each error message unconditional. The shared field rules stay
shared.

**What Stays:** All three validation rules, all warning text, contract item 10.

**Behavior Impact:** None **Risk:** Low

**Files Likely Affected:** `GameRepository.cpp`, `GameService.h/.cpp`

**Testing:** `GameRepositoryTest.cpp` covers insert-with-preassigned-ID, update-without-ID, blank title, and
non-positive Steam App ID. Fully protected. **Dependencies:** None **Recommendation:** Nice cleanup **Approval:**
AWAITING APPROVAL

---

### Medium Refactors

---

**ID:** M1 **Category:** MEDIUM

**Problem:** The two repositories independently reimplement the same query-assembly mechanics.

**Location:** [GameRepository.cpp:104-191](src/core/database/GameRepository.cpp#L104-L191),
[SessionRepository.cpp:303-420](src/core/database/SessionRepository.cpp#L303-L420)

**Evidence:** Both files contain: a `QStringList predicates` + `QList<QPair<QString, QVariant>> bindings` pair; an
`IN (...)` placeholder generator (`appendIdPredicate` vs `appendInPredicate`) that differ only in parameterization; an
identical `WHERE` join; identical `ORDER BY` + `ASC`/`DESC` assembly; byte-identical
`LIMIT`/`OFFSET` blocks *including the same `LIMIT -1` SQLite workaround and its comment*; and the same
`for(const auto& [placeholder, value] : bindings) sqlQuery.bindValue(...)` loop.

**Proposed Change:** Add a small `core/database/SqlQueryBuilder` (header + source, ~80 lines) owning:
`addPredicate(sql, binding)`, `addInPredicate(column, prefix, values)`, `setOrderBy(expression, direction)`,
`setLimitOffset(limit, offset)`, and `buildAndBind(QSqlQuery&)`. Both repositories keep their own column mapping, their
own `orderColumn()` switch, their own domain-specific predicates (`normalizedTimestampExpression`
stays in `SessionRepository`), and their own row readers.

**Why:** This is the clearest accidental duplication in the codebase. The `LIMIT -1` workaround exists twice, and a bug
in one copy would not be visible from the other. The extracted piece is genuinely reusable and has no domain knowledge,
which is what makes the boundary a good one rather than an arbitrary split.

**What Stays:** `GameQuery`/`SessionQuery`, both `query()` signatures, every predicate's semantics, all SQL text, the
row-mapping functions, `sessionFromQuery`'s corrupted-row skipping.

**Behavior Impact:** None — the generated SQL should be byte-identical. **Risk:** Moderate. This is the highest-risk
item on the list because it touches the SQL of both repositories. It should be done only *after* confirming the existing
repository tests pass, and I would verify by logging the generated SQL before and after.

**Files Likely Affected:** new `SqlQueryBuilder.h/.cpp`, `GameRepository.cpp`, `SessionRepository.cpp`,
`src/core/CMakeLists.txt`

**Testing:** `GameRepositoryTest.cpp` (557 lines) and `SessionRepositoryTest.cpp` (938 lines) cover filtering, sorting,
limit/offset, and boundary cases extensively — this is exactly the safety net that makes the refactor viable. **I would
add a characterization test asserting the generated SQL string for one representative query of each type before touching
either file.**
**Dependencies:** S1 first (both headers change anyway). **Recommendation:** Nice cleanup — worth doing, but not before
the safer items. **Approval:** AWAITING APPROVAL

---

**ID:** M2 **Category:** MEDIUM

**Problem:** `SessionSource` and `SessionStatus` are converted to strings in three places using two different spelling
conventions, and the direction that production actually persists lives in an anonymous namespace.

**Location:** [Session.cpp:9-35,54-67](src/core/domain/Session.cpp#L9-L35),
[SessionQuery.cpp:24-50](src/core/domain/query/SessionQuery.cpp#L24-L50),
[SessionRepository.cpp:25-66](src/core/database/SessionRepository.cpp#L25-L66)

**Evidence:** Three independent copies:

1. `Session.cpp` anonymous namespace — `"Automatic"`, `"Active"` (capitalized, for `QDebug`), plus *public*
   `sessionSourceFromString`/`sessionStatusFromString` that **throw** `std::invalid_argument`.
2. `SessionQuery.cpp` anonymous namespace — `toStringValue()` overloads producing the identical capitalized strings,
   duplicating copy 1 verbatim for the same `QDebug` purpose.
3. `SessionRepository.cpp` anonymous namespace — `"automatic"`, `"active"` (lowercase, the actual persisted values) plus
   `std::optional`-returning parsers.

Copies 1 and 2 are the same function written twice. Copy 3 is the one that matters for the database, is the only one
returning `std::optional` rather than throwing, and is invisible outside one `.cpp`.

**Proposed Change:** Two clearly-named, separately-purposed pairs in `core/domain/Session.h/.cpp`:

- **Display:** `QString toDisplayString(SessionSource)` / `(SessionStatus)` — the capitalized forms. Used by both
  `QDebug` operators; delete the duplicate in `SessionQuery.cpp`.
- **Persistence:** `QString toDatabaseString(...)` / `std::optional<SessionSource> sessionSourceFromDatabase(...)`
  — the lowercase forms, moved up from `SessionRepository.cpp` with a comment stating that these values are written to
  disk and are schema-`CHECK`-constrained.

**Leave `sessionSourceFromString`/`sessionStatusFromString` exactly as they are.** Contract item 9 pins their exact
accepted spellings and their throwing behavior, and `CONTRACT_CHANGES.md` item 1 explicitly keeps
`Session.cpp` in test scope for them. They are tested-only today, but they are contracted, not dead.

**Why:** One enum, one mapping table per purpose, with the persistence mapping visible and labelled as disk format. The
display duplication disappears entirely. This also makes it obvious to a future reader why two different spellings
exist — which is currently discoverable only by reading three files.

**What Stays:** All three sets of string values, both `QDebug` operators' output, the throwing converters and their
contract, `SessionRepository`'s corrupted-row skipping behavior, the schema `CHECK` constraints.

**Behavior Impact:** None **Risk:** Low

**Files Likely Affected:** `Session.h/.cpp`, `SessionQuery.cpp`, `SessionRepository.cpp`

**Testing:** `SessionTest.cpp` covers the throwing converters (untouched). `SessionRepositoryTest.cpp` covers
round-tripping every source/status and skipping corrupted enum rows via `PRAGMA ignore_check_constraints`. Well
protected. **Dependencies:** None **Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** M3 **Category:** MEDIUM

**Problem:** `SessionService` owns both the session CRUD/lifecycle *and* a time-based process-detection state machine,
and the state machine cannot be tested without a database.

**Location:** [SessionService.h:130-141,180-241](src/application/services/local/SessionService.h#L130),
[SessionService.cpp:277-333,411-486](src/application/services/local/SessionService.cpp#L277)

**Evidence:** `SessionService` is 487 lines with five distinct jobs: query wrappers, CRUD with signal emission,
restore-and-repair, the automatic-tracking state machine, and candidate selection. The state machine owns four members
that nothing else touches — `pendingGameId_`, `gameOpenDuration_`, `gameClosedDuration_`, plus `kStartGracePeriod`/
`kEndGracePeriod` — and `resetPendingStart()`/`resetAutomaticTracking()` exist only to service it. Because it is fused
into the service, testing "does 30 seconds of continuous detection start a session?" currently requires a live SQLite
database, a `GameService`, a repository, and an injected clock.
`SessionServiceTest.cpp` is 1142 lines, the largest file in the repository, largely for this reason.

**Proposed Change:** Extract `application/services/local/AutomaticSessionTracker` — a plain class (no
`QObject`, no database, no signals) with one method:

```cpp
enum class TrackingAction { None, Start, Stop };
struct TrackingDecision { TrackingAction action{TrackingAction::None}; std::optional<Game> game; };

TrackingDecision advance(const std::vector<ProcessInfo>& processes,
                         std::chrono::seconds elapsed,
                         bool sessionIsActive,
                         const Game* activeGame);
```

It owns the grace-period counters, the constants, and `selectDetectedGame`. `SessionService::updateAutomaticTracking`
becomes ~15 lines: call `advance()`, then `startAutomaticSession()` or `endActiveSession()` on the result.
`resetAutomaticTracking()` forwards to the tracker.

**Why:** This is the one place where the current structure genuinely impedes testing. The grace-period logic is the most
intricate behavior in the application and is exactly the kind of thing you want to test exhaustively — detection
flapping, a different game appearing mid-grace-period, elapsed values that skip past the threshold — and after this
change each of those is a three-line test with no database. It also cuts the largest service down to a single coherent
responsibility. `SessionService` keeps ownership of *what to do* with a decision; the tracker only decides.

**What Stays:** `SessionService`'s entire public API, both grace-period durations, the exact candidate precedence
(pending → Steam → path → lowest ID), `activeSession_`/`activeGame_` ownership, all lifecycle signals, contract item 24.

**Behavior Impact:** None intended — this is a pure move of existing logic. **Risk:** Moderate. The state machine is
subtle; the risk is in transcription, not in design.

**Files Likely Affected:** new `AutomaticSessionTracker.h/.cpp`, `SessionService.h/.cpp`,
`src/application/CMakeLists.txt`, `tests/CMakeLists.txt`, new `AutomaticSessionTrackerTest.cpp`

**Testing:** `SessionServiceTest.cpp` covers `updateAutomaticTracking` today and must keep passing unchanged — that is
the safety net. **New unit tests for the tracker should be written as part of this change**, not before; the point of
the refactor is that they become possible. **Dependencies:** Pairs naturally with M4. Do M4 first if you approve both.
**Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** M4 **Category:** MEDIUM

**Problem:** The rule "Steam identity beats path identity" is implemented twice, in two layers, and the copies can
silently diverge.

**Location:** [ProcessHelpers.cpp:46-66](src/core/process/ProcessHelpers.cpp#L46-L66),
[SessionService.cpp:411-452](src/application/services/local/SessionService.cpp#L411-L452)

**Evidence:** `ProcessHelpers::matchTrackedGame` looks up `trackedSteamGames` by App ID, falls back to
`trackedPathGames` by executable path, and confirms with `processMatchesGame`.
`SessionService::selectDetectedGame` does the *same three steps in the same order* against the same two hashes — but
inline, so that it can also record whether the hit was a Steam match (priority 0) or a path match (priority 1).
`matchTrackedGame` is exercised by `ProcessHelpersTest.cpp` and is documented as the canonical matcher; the copy inside
`SessionService` is the one production actually uses for session starts. Contract item 33 defines this precedence once —
it should exist in code once.

**Proposed Change:** Change `matchTrackedGame` to return the match kind alongside the game:

```cpp
enum class MatchKind { None, SteamAppId, ExecutablePath };
struct TrackedGameMatch { const domain::Game* game{nullptr}; MatchKind kind{MatchKind::None}; };
```

`selectDetectedGame` then calls it per process and derives its priority from `kind`, keeping its own deduplication,
pending-game preference, and lowest-ID tie-break — all of which are session-selection policy and belong in
`SessionService` (or in the M3 tracker).

**Why:** Contract item 33 is a single rule; it should have a single implementation, and it should be the one that
already has dedicated tests. This is an abstraction that removes real duplication rather than adding indirection.

**What Stays:** The precedence rule itself, `processMatchesGame`, the documented pointer-lifetime contract,
`selectDetectedGame`'s tie-breaking policy, contract item 24.

**Behavior Impact:** None intended. **Risk:** Low — the pointer-lifetime documentation on `matchTrackedGame` already
covers the returned reference.

**Files Likely Affected:** `ProcessHelpers.h/.cpp`, `SessionService.cpp` (or `AutomaticSessionTracker.cpp`)

**Testing:** `ProcessHelpersTest.cpp` (264 lines) covers matching precedence including the mismatched-Steam-ID case;
`SessionServiceTest.cpp` covers multi-candidate selection. Extend `ProcessHelpersTest` to assert the new
`MatchKind`. **Dependencies:** None, but do this *before* M3 if both are approved. **Recommendation:** Do now
**Approval:** AWAITING APPROVAL

---

**ID:** M5 **Category:** MEDIUM

**Problem:** Importing a Steam library performs one full-table query and one complete index rebuild *per game*.

**Location:** [GameService.cpp:161-190](src/application/services/local/GameService.cpp#L161-L190) with
[GameService.cpp:76-84,105-128](src/application/services/local/GameService.cpp#L76-L84)

**Evidence:** `onSteamGamesReceived` loops over the Steam response; for each entry it runs a `search()` by Steam App ID
(one SQL round trip), then calls `addGame()`, which calls `syncGamesWithDatabase()` — which issues `listTrackedGames()`
(a full table scan) and rebuilds both `QHash` indexes from scratch — and then emits
`gameAdded`. For a 500-game library that is ~1000 queries and 500 full index rebuilds, with the rebuild cost growing as
the library grows: textbook accidental quadratic behavior on the one operation guaranteed to run against a large data
set. It is also the only place in the codebase where a bulk operation exists.

**Proposed Change:** Restructure `onSteamGamesReceived` to:

1. Issue **one** query for all existing Steam App IDs (`GameQuery` with no filters, or a dedicated
   `steamAppIds()` lookup) and build a local `QSet<int>`.
2. Insert each genuinely-new game through the repository, wrapped in a single transaction.
3. Call `syncGamesWithDatabase()` **once** at the end, and emit `gameAdded` for each inserted game.

Keep `addGame()` exactly as it is for the single-add case.

**Why:** This is a measurable inefficiency on a realistic input, not a micro-optimization, and the fix makes the code
*simpler* — one query and one rebuild instead of a hidden per-item cost buried two calls deep. It also removes the
reentrancy described in B2 from the bulk path.

**What Stays:** Contract item 12 in full — the existence check still consults the complete database rather than only the
tracked cache, existing rows (including untracked ones and local titles) are still left entirely unchanged, and
duplicate App IDs are still never merged. `addGame`'s signature and behavior. The `gameAdded`
signal for every inserted game.

**Behavior Impact:** Potential — signal *ordering* changes (all inserts, then all `gameAdded` emissions, instead of
interleaved). Since `gameAdded` drives artwork downloads, the observable effect is that artwork requests are queued
after the import completes rather than during it. I consider this an improvement, but it is a behavior change and needs
explicit sign-off. **Risk:** Moderate

**Files Likely Affected:** `GameService.h/.cpp`; possibly a bulk-insert helper on `GameRepository`

**Testing:** `GameServiceTest.cpp` covers `onSteamGamesReceived` including the "leave existing rows unchanged"
contract. **Add a characterization test first** asserting `gameAdded` emission count and the resulting rows for a mixed
new/existing payload, since emission ordering is what changes. **Dependencies:** Related to B2; if you approve the
queued-connection fix for B2, do it first. **Recommendation:** Do now **Approval:** AWAITING APPROVAL

---

**ID:** M6 **Category:** MEDIUM

**Problem:** The migration ledger is validated twice with two different code paths, using N+1 queries.

**Location:** [DatabaseMigrator.cpp:81-148](src/core/database/DatabaseMigrator.cpp#L81-L148)

**Evidence:** `validateMigrationLedger()` reads every ledger row and checks each recorded version against the compiled
list, including a name-mismatch check. Then `applyPendingMigrations()` loops over the compiled migrations calling
`isApplied()`, which issues *another* `SELECT` per migration and repeats the same name comparison with a near-identical
warning message. The two checks are consistent today, but they are two implementations of one rule, and the
per-migration query is unnecessary because the full ledger was already read moments earlier.

**Proposed Change:** Read the ledger once into a `QHash<int, QString>` (version → name). Validate it against the
compiled list. Then drive the apply loop from that map with no further queries. `isApplied()` becomes a lookup rather
than a query, or disappears.

**Why:** Removes a duplicated correctness rule and N queries from every application startup, and makes the migration
flow read top-to-bottom: read ledger → validate → apply missing.

**What Stays:** Contract item 31 in full — version *and* name must match; unknown/newer versions still fail startup;
pending migrations still apply transactionally in compiled order. All warning messages. The
`schema_migrations` table shape.

**Behavior Impact:** None **Risk:** Low — this runs on every startup, but `DatabaseMigratorTest.cpp` (321 lines) covers
it directly.

**Files Likely Affected:** `DatabaseMigrator.h/.cpp`

**Testing:** `DatabaseMigratorTest.cpp` covers name mismatch, unknown version, newer-than-binary version, and partial
application. Well protected. **This refactor touches the schema-compatibility gate, so run the full database test label
before and after.**
**Dependencies:** None **Recommendation:** Nice cleanup **Approval:** AWAITING APPROVAL

---

**ID:** M7 **Category:** MEDIUM

**Problem:** A GUI widget decides when to start network downloads.

**Location:** [GameCard.cpp:44](src/gui/game_card/GameCard.cpp#L44)

**Evidence:** `GameCard`'s constructor ends with
`if(artworkService_ != nullptr && gameId_ > 0) { static_cast<void>(artworkService_->getGameArtwork(game)); }`.
Constructing a card therefore issues up to three Steam CDN requests as a side effect of laying out a widget.
`LibraryView::displayAllGames()` constructs one card per game — it currently passes no artwork service, so this does not
fire there, but the coupling means it would begin firing the moment someone wires the service through, and clicking
"Refresh" would re-trigger the whole library's downloads.

You flagged exactly this category in your instructions: *"Point out cases where business logic has accidentally moved
into the GUI."* This is the clearest instance.

**Proposed Change:** Make `GameCard` passive: it displays local artwork if present, shows the placeholder otherwise, and
refreshes when `artworkAvailable` fires for its game ID (all of which it already does). Move the *decision to fetch* to
the owner — either the runtime wiring that already connects `gameAdded` →
`getGameArtwork`, or an explicit `LibraryView` action.

**Why:** Widget construction should not perform I/O, and "when do we fetch artwork?" is application policy that a future
GUI redesign should not have to re-implement. This is the one GUI change that fixes a genuine backend coupling rather
than polishing presentation.

**What Stays:** `GameCard`'s appearance, its `sizeHint`/`heightForWidth` behavior, the placeholder fallback, the
`artworkAvailable` refresh connection, `GameArtworkService`'s API.

**Behavior Impact:** Intentional — cards would no longer trigger downloads on construction. Practically invisible today
(only `LiveWindow` passes the service, for a single card). **Risk:** Low

**Files Likely Affected:** `GameCard.h/.cpp`, `LiveWindow.cpp`, possibly `GameLogRuntime.cpp`

**Testing:** No GUI tests exist. `GameArtworkServiceTest.cpp` covers the service side. Manual `--live`
verification. **Dependencies:** None **Recommendation:** Defer — real, but per your scope guidance the GUI is
scaffolding. Worth doing before the GUI is built out for real, not before the backend work. **Approval:** AWAITING
APPROVAL

---

**ID:** M8 **Category:** MEDIUM

**Problem:** Steam API JSON is parsed into domain objects inside the service that also owns persistence policy.

**Location:** [GameService.cpp:161-190](src/application/services/local/GameService.cpp#L161-L190)

**Evidence:** `onSteamGamesReceived` reads `"appid"` and `"name"` from raw `QJsonObject`s, validates them, and
constructs `Game` values — mixed in with the same loop that runs the existence query and calls `addGame`. Your
instructions list "mapping external API objects into domain objects" as a natural extraction boundary. Testing
"does a malformed Steam entry get skipped?" currently requires a database.

**Proposed Change:** Add a free function
`std::vector<Game> gamesFromSteamOwnedGames(const QJsonArray&)` (in
`application/services/web/SteamGameMapper.h/.cpp`) that performs only the shape validation and mapping.
`GameService::onSteamGamesReceived` keeps the "does it already exist / should it be inserted" policy.

**Why:** Separates parsing an external format from deciding what to persist, and makes the parsing directly
unit-testable with no database. Small and self-contained.

**What Stays:** The validation rules (`appid > 0`, non-blank trimmed title), the skip-non-object behavior, contract item
12's persistence policy in `GameService`.

**Behavior Impact:** None **Risk:** Low

**Files Likely Affected:** new `SteamGameMapper.h/.cpp`, `GameService.cpp`, `src/application/CMakeLists.txt`,
`tests/CMakeLists.txt`

**Testing:** `GameServiceTest.cpp` covers malformed entries today. Add a small mapper unit test. **Dependencies:** Do
together with M5 — they touch the same function. **Recommendation:** Nice cleanup **Approval:** AWAITING APPROVAL

---

**ID:** M9 **Category:** MEDIUM

**Problem:** Two 45-line credential dialogs differ only in their strings, and route through a signal to reach an object
the same function already holds a pointer to.

**Location:** [MainWindow.cpp:95-183](src/gui/main_window/MainWindow.cpp#L95-L183), with
[MainWindow.cpp:59-76](src/gui/main_window/MainWindow.cpp#L59-L76)

**Evidence:** `onAddSteamApiKey` and `onAddSteamPlayerId` are near-identical: same dialog construction, same layout,
same button box, same accepted-handler shape. They differ in the window title, the explanation text (which for the
player ID dialog incorrectly says *"Enter your Steam player API key below. You can obtain a key from Steam's developer
page"*), the placeholder, and which signal is emitted. Each signal is then connected in the constructor to a lambda that
calls `credentialService->setSecret(...)` — the slot could simply call it.

**Proposed Change:** One private helper
`std::optional<QString> promptForSecret(const QString& title, const QString& explanation, const QString& placeholder)`.
Both slots call it and then `setSecret` directly. Delete the two signals and their lambda connections. Fix the player-ID
explanation text.

**Why:** Duplicated GUI setup is on your duplication-review list, and removing the signal round-trip removes an
indirection that obscures a straight-line call.

**What Stays:** Both menu actions, the password echo mode, the trim-and-reject-empty rule, the modal behavior, which
keychain key each writes.

**Behavior Impact:** Intentional — the two public signals disappear. Nothing else connects to them. **Risk:** Low

**Files Likely Affected:** `MainWindow.h/.cpp`

**Testing:** None exists. Manual `--gui` verification that both dialogs still store their secret. **Dependencies:** None
**Recommendation:** Defer — per your GUI scope guidance. Cheap if you want it bundled with S9. **Approval:** AWAITING
APPROVAL

---

## 4. Large Refactors

**None recommended.**

Your instructions ask that a Large recommendation answer "what concrete problem exists today?" — and after tracing the
workflows, I could not answer that question for any architectural change. Two candidates were considered and rejected:

**Decomposing `GameLogRuntime` into a separate tracking subsystem.** The runtime is 182 lines, has one responsibility
(own the long-lived objects and pump the tracking loop), and its ownership model is already explicit and correct.
Extracting a `TrackingCoordinator` would move `update()`'s eight lines somewhere else and add a layer. If M3 and M4
land, `update()` gets *simpler*, not more tangled. Rejected: no concrete problem.

**Reworking the repository/service boundary.** Every candidate complaint I checked turned out to be deliberate and
documented — service-layer active-session checks duplicating the `one_active_session` index (contract item 19, defense
in depth), corrupted-row handling for states the schema prevents (item 29),
`SessionService` holding cached active state rather than re-querying. The boundary is clean: no SQL escapes a
repository, no domain rule lives in one that isn't a persistence invariant. Rejected: nothing is wrong.

The problems in this codebase are consistency, duplication, and two or three localized structural issues. They are all
reachable with Small and Medium changes.

---

## 5. Probable Bugs

These are behavioral defects, listed separately from refactoring. **Each needs separate approval, and each changes
behavior.**

**B1 — One test is currently failing; the safety net is incomplete.**
The most recent recorded CTest run (`build/TestLog.txt`, dated Aug 19) reports **16 of 17 passing**, with
`SteamApiServiceTest::getOwnedGames_neverLogsTheApiKeyOrTheQueryString` failing at
[SteamApiServiceTest.cpp:453](tests/application/services/web/SteamApiServiceTest.cpp#L453) on
`'!messages.isEmpty()' returned FALSE` — the installed message handler captured nothing, so the credential-leakage
assertions never ran. The request itself succeeded (the preceding `QVERIFY(received)`
passed), which points at the logging-rule/message-handler interaction rather than at the service. **This should be
diagnosed and fixed before any refactoring begins**, since it is the test guarding contract item 7. I have not fixed it,
only identified it.

**B2 — Synchronous signal cycle inside `GameService::addGame`.**
`addGame` emits `gameAdded` → `GameArtworkService::getGameArtwork` → emits `artworkAvailable`/`artworkUnavailable`
→ the runtime lambda calls `GameService::setHasArtwork` → `updateGame` → a second repository write, a second full index
rebuild, and a `gameUpdated` emission — **all before `addGame` returns**. Consequences: the caller's
`Game&` is stale on return (its `hasArtwork` no longer matches the row), every insert writes twice, and the index
rebuilds twice. Also note `getGameArtwork` is `[[nodiscard]] bool` yet connected as a slot, so its result is silently
discarded. Suggested fix: make the runtime's artwork↔game connections `Qt::QueuedConnection`, and re-read the game in
`addGame` or document the staleness. Behavior change; needs approval.

**B3 — `SteamApiService` can wedge permanently.**
`getOwnedGames()` sets `requestInProgress_ = true` and requests two secrets. If one callback never arrives — a keychain
that never answers, a `QKeychain` job that neither succeeds nor errors — nothing ever resets the flag, and every
subsequent `getOwnedGames()` fails with "already in progress" for the process lifetime. There is no timeout. Contract
item 6 states *"The service must not remain waiting indefinitely"*; the contract is satisfied for empty/blank secrets
but not for a silent callback. Suggested fix: a `QTimer` guard that fails the request after a bounded wait. Behavior
change; needs approval.

**B4 — Injected `QNetworkAccessManager` receives replies it did not issue.**
`GameArtworkService` connects to `QNetworkAccessManager::finished`, which fires for *every* reply on that manager. With
the service-owned manager (production) that is safe. With the injected-manager constructor, any reply from another user
of the same manager reaches the artwork handler, fails the property check, logs
"Missing artwork metadata", and is `deleteLater`'d **by the artwork service** — deleting a reply another component may
still be reading. Suggested fix: connect per-reply (`&QNetworkReply::finished`) as
`SteamApiService` already does. Low likelihood today; genuinely dangerous if a manager is ever shared.

**B5 — GUI dereferences service pointers documented as nullable.**
`LibraryView`, `CalendarView`, `LiveWindow`, and `MainWindow` all call `runtime.getXService()->...` with no null check,
while `GameLogRuntime` explicitly documents these as returning `nullptr` when database initialization fails. Currently
unreachable because `main.cpp` exits when `start()` fails — but nothing enforces or documents that precondition at the
window constructors. Suggested fix: document the precondition on the window constructors (cheapest), or assert.

**B6 — `LiveWindow` timer drifts.**
`updateTimerText()` adds one second to a `QTime` per tick rather than recomputing from the session start, so the
displayed elapsed time drifts whenever the event loop is delayed. GUI-only, low priority.

**B7 — `CalendarView` never clears stale date highlights.**
`onPageChanged` calls `setDateTextFormat` for each session found but never resets previously-set formats, so highlights
persist after a session is deleted. GUI-only, low priority.

---

## 6. Documentation Problems

**D1 — `README.md` is corrupted and badly out of date.** It contains `dd This repository currently contains
the initial project scaffold only` (which is no longer true), the garbled line
`` - `cmake`gamegamegamegamegameecific gam ``, a mangled `libprocs` dependency line with a stray `2` and a tracking URL,
and — from line ~50 onward, roughly 380 lines — **an entire pasted AI implementation plan**
("Here is Claude's plan: # Test Suite Expansion — GameLog") left inside the project's front page. For a portfolio
project this is the first thing a reader sees. Recommend: rewrite as a real README and move the plan out of the
repository.

**D2 — `docs/architecture.md` describes an architecture that does not exist.** It documents two executables
(`gamelog-runtime` and `gamelog`) communicating over local IPC. There is one executable, `gamelog`, with three run modes
and no IPC. `docs/GameLog_Software_Design_Document.md` is accurate and supersedes it. Recommend:
delete `architecture.md` or replace it with a pointer to the SDD.

**D3 — `SessionDocument`'s documentation contradicts the schema and the GUI.** The struct documents
`htmlContent` as *"HTML payload written to disk or the database"*, but migration 002 renamed the column to
`content` and 003 removed the `format` column entirely, and `TextEditor::getMarkdown()` means the GUI writes
**Markdown**. Meanwhile `Session::notes` documents no format at all. The note format should be stated in one place. (See
also X1 — the struct itself is unused.)

**D4 — `CredentialService` documentation defects.** The class has no class-level comment despite being an architectural
component. Its constructor carries a stray `@brief The key used to store the Steam Player Name in
the keychain.` — a comment for a constant that isn't there. And `kSteamPlayerIdKey` has the value
`"player_id_key"` while `kSteamApiKey` is `"steam_api_key"`; the inconsistency is cosmetic *but the value is a persisted
keychain key*, so renaming it would strand existing users' stored credentials. Recommend documenting why it stays rather
than changing it.

**D5 — Missing class-level documentation** on `ProcessInfo`, `SteamProcessInspector`, `SteamApiService`,
`ArtworkType`, `GameArtworkService`, `TextEditor`, and `LiveWindow`, while most peers have it.

**D6 — An important invariant is documented only in the contract file.** The `one_active_session` partial unique index
is the *primary* enforcement of single-active-session; `SessionService::hasOtherActiveSession()`
and `restoreActiveSession()`'s multi-row repair are deliberate defense-in-depth (contract item 19). Nothing in the code
says so, so both read like redundant checks. A two-line comment in `SessionService` pointing at the index would prevent
someone "simplifying" them away.

**D7 — Doxygen mechanics**: see S6 (typos, misplaced blocks, inconsistent indentation).

---

## 7. Dead or Obsolete Code

**X1 — `SessionDocument` is entirely unused.** `src/core/domain/SessionDocument.h/.cpp` define a struct and a
`QDebug` operator with **zero references anywhere in `src/` or `tests/`**. `SessionRepository` reads and writes the
`session_documents` table using raw `QString` notes and never constructs this type. Either delete both files (and the
`CMakeLists.txt` entry), or use it as the repository's document type. Deleting is the smaller change.

**X2 — Three logging categories are declared, defined, and never used**: `gamelogGuiLog`, `gamelogProcessLog`,
`gamelogGameServiceLog`. A fourth, `gamelogSessionServiceLog`, has exactly one call site — inside
`CredentialService`. Addressed by S2 (wire them up rather than delete).

**X3 — Commented-out code**: `ProcfsProcessSource.cpp:76` (a disabled `qInfo` line) and
`src/core/CMakeLists.txt:7` (`# sessions/SessionManager.cpp`, a file that does not exist).

**X4 — `CalendarView::gameService_`** is assigned in the constructor and never read.

**X5 — Duplicated CMake block**: `CMakeLists.txt:48-54` repeats `include(CTest)`, `include(GNUInstallDirs)`,
`find_package(Qt6 ...)`, and `pkg_check_modules(PROC2 ...)` from lines 38–44. Addressed by S11.

**X6 — `MainWindow::steamAPIKeyEntered` / `steamPlayerIdEntered`** exist only to hop to a lambda in the same
constructor. Addressed by M9.

**X7 — Unused declarations**: `QListWidget` forward declaration in `MainWindow.h`; `<qloggingcategory.h>` and
`logging/LoggingCategories.h` in `MainWindow.cpp`. Addressed by S4.

---

## 8. Future Ideas — explicitly *not* part of this pass

- A timeout/cancellation mechanism for `SteamApiService` (beyond the minimal B3 fix).
- Bulk/transactional insert API on `GameRepository` (M5 needs only a local version).
- Windows/macOS `ProcessSource` implementations. The `ProcessSource` seam is already sufficient; nothing in the current
  design blocks this, and `ProcfsProcessSource` is the only Linux-specific file (`ProcessHelpers`'
  `/proc/<pid>/environ` reader is the one other spot). **No portability refactor is needed today.**
- Persisting the note format explicitly (a schema change — Medium impact minimum).
- A real GUI: library filtering, per-game summary screens, stylesheets.
- Artwork cache eviction / re-download policy.
- Any GUI test infrastructure.

---

## 9. Proposed Implementation Order

Dependencies and risk determine the order; behavior-preserving work comes first so the codebase is easier to work in
before anything structural moves.

**Phase 0 — Restore the safety net (do this before anything else)**

1. Diagnose and fix **B1** (the failing Steam API test).
2. **S11** (enable warnings on all targets) — run it, report the warning list, fix them.
3. Confirm a clean full `ctest` run. Nothing below proceeds until this holds.

**Phase 1 — Safe cleanup (no behavior change, mechanical)**

4. **S1** (global `using` removal) — first, because S4/S9 touch the same lines.
5. **S4**, **S5**, **S6** (header hygiene, `Q_SIGNALS`/`Q_SLOTS`, doc typos) — one pass over the headers.
6. **S2**, **S3** (logging categories, log noise).
7. **S7**, **S8** (`std::pmr::map`, duplicated constructors).
8. **S10** (`CalendarView` foreign-signal emit).
9. **X1**, **X3**, **X4** (dead code removal).
10. **D1**, **D2** (README and `architecture.md`).

**Phase 2 — Local structural refactors**

11. **M2** (enum↔string consolidation).
12. **S13**, **S12** (validation split, ID types).
13. **M6** (migrator ledger).
14. **M4** (unify process→game matching) — before M3.

**Phase 3 — Component-level refactors**

15. **M3** (extract `AutomaticSessionTracker`) + its new unit tests.
16. **M5** + **M8** (Steam sync batching and mapper) — together; add the characterization test first.
17. **M1** (SQL query builder) — last of the structural work, highest risk, and it benefits from everything above being
    settled. Characterization test on generated SQL first.

**Phase 4 — Optional / behavior-changing (each separately approved)**

18. **B2** (queued artwork connections), **B3** (Steam request timeout), **B4** (per-reply connections).
19. **S9**, **M7**, **M9** (GUI namespace, artwork policy, dialog helper).
20. **B5**, **B6**, **B7** (GUI robustness).

---

## 10. Verification (per approved item)

Every refactor follows the process from your instructions:

1. `cmake --build build && ctest --test-dir build --output-on-failure` — confirm green *before* starting.
2. Add characterization tests first where flagged (M1, M5).
3. Make the smallest coherent change.
4. Rebuild the affected targets; run the relevant CTest label (`ctest -L database`, `-L process`, `-L application`).
5. Run the full suite.
6. Fix any warnings the change introduces.
7. Summarize exactly what changed.

GUI-affecting items additionally require a manual launch of `./build/src/gamelog --gui` and `--live`, since no GUI tests
exist.

Before starting each approved item I will restate:

```
Refactor:
Files expected to change:
Behavior expected to change:
Tests protecting the change:
```

**No item is implemented without its number appearing in your approval list. Approving one item does not approve any
related item.**
