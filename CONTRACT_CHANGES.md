# Contract Changes and Test Boundaries

The numbering below matches the original 36 clarification questions. Each item states the boundary that new tests should
enforce.

## Corrupted persistence test setup

Some contracted behaviors defend against persisted states that the current SQLite schema prevents from being created.
Tests covering those states may deliberately bypass database constraints when constructing fixtures.

In particular:

- `PRAGMA ignore_check_constraints = ON` may be used to create invalid enum and invalid duration rows for corrupted-row
  query tests.
- `PRAGMA foreign_keys = OFF` may be used to construct orphaned-session fixtures.

These pragmas are test-fixture mechanisms only. They do not represent supported production database behavior, and
production code must never enable them.

One constraint is deliberately *not* bypassable: tests must not drop `one_active_session`. See item 19.

---

1. **Test-file scope** — Treat `main.cpp` and every translation unit with application behavior as in scope. A `.cpp`
   whose only behavior is `QDebug operator<<` or logging-category definition does not require a dedicated behavioral
   test. `Session.cpp` remains in scope because it also parses strings.

2. **External-dependency seams** — Production behavior is unchanged, but tests may inject or override: keychain job
   creation in `CredentialService`; `QNetworkAccessManager` in both web services; the `SessionService` clock; the
   runtime `ProcessSource`; and the `SteamProcessInspector` Steam App ID reader.

3. **Linux process environment** — Production and the live integration test may assume Linux, `/proc`, and libproc2.
   Exact process contents must not be asserted. Higher-level lifecycle tests use deterministic process snapshots and
   injected readers/sources.

4. **Run-mode parsing** — Startup succeeds only with exactly one argument and only when it is `--headless`, `--gui`, or
   `--live`. No mode, repeated modes, mixed modes, and any unrecognized/extra argument fail before application startup.

5. **Credential validation** — Empty or whitespace-only keys fail for set/get/remove. Empty or whitespace-only secrets
   fail `setSecret`; they do not remove an existing value. Removal remains an explicit `removeSecret` operation.

6. **Steam credential completion** — An empty or whitespace-only API key or player ID immediately fails the active
   request, clears request state, and emits a recoverable, credential-specific error. The service must not remain
   waiting indefinitely.

7. **Steam API authentication placement** — The API key appears only in the `key` query parameter. No `x-webapi-key`
   header is set. Service logs use the endpoint without its query string and never print the key.

8. **Steam response shape** — `{"response":{"games":[]}}` is a successful empty library and emits
   `ownedGamesReceived([])`. The root must be an object, `response` must be an object, and `games` must be an array.
   Missing or non-array `games`, including `{"response":{}}`, emits `requestFailed`.

9. **Session enum parsing** — Preserve exact current spellings only: lowercase and leading-capital forms. Do not trim
   and do not add general case-insensitivity. Empty, padded, uppercase, or unknown strings throw
   `std::invalid_argument`.

10. **Game persistence validation** — Insert requires `id == 0`, a title nonblank after trimming, and either no Steam
    App ID or a positive Steam App ID. Update requires a positive existing ID and the same field validation.
    `hasArtwork` is a non-null boolean and defaults false.

11. **Game uniqueness** — Database ID and non-null Steam App ID remain unique. A duplicate Steam App ID insert fails
    through the database constraint; it is not an update/merge operation. Executable name/path remain non-unique for
    this revision.

12. **Steam game synchronization** — Before insertion, search the complete database by Steam App ID, not only the
    tracked cache. Any existing row—including an untracked row—is left entirely unchanged. Local title, tracking state,
    and other local fields are preserved.

13. **Tracked-index collisions** — Steam App ID collisions should be impossible after persistence validation. Duplicate
    executable paths retain the existing `QHash::insert` replacement behavior and are explicitly not established as a
    strengthened contract in this change set.

14. **Artwork method result** — `getGameArtwork` returns true only when a valid local `cover.jpg` exists when the call
    returns. Merely queueing one or more downloads returns false.

15. **Artwork completeness** — Only a nonempty, decodable `cover.jpg` currently establishes `Game.hasArtwork`. A
    directory alone, an empty/invalid cover, or header/logo without a cover does not. Missing/invalid cover files are
    retried on later calls.

16. **Artwork signaling/state** — Availability signals include `(gameId, ArtworkType)`. Emit availability for a
    validated local cover and for each successfully validated/written download. Emit unavailability for
    missing/invalid/currently unavailable files. Only `Cover` signals alter persisted `Game.hasArtwork`; header/logo
    partial success does not.

17. **Artwork response validation** — Before writing or reporting success, bytes must decode through `QImage` as the
    expected JPEG (cover/header) or PNG (logo). Empty, mislabeled-by-content, or undecodable payloads fail and are not
    retained as successful files.

18. **Session time range** — `getSessionsInTimeRange(start, end)` is the half-open interval `[start, end)`:
    `startedAtOrAfter >= start` and `startedBefore < end`.

    Persisted timestamps participating in SQL range comparisons must use one canonical sortable representation.
    `SessionRepository` currently writes UTC timestamps using `Qt::ISODateWithMs`; legacy or deliberately corrupted rows
    using `Qt::ISODate` without milliseconds must either be normalized before comparison or handled by a query
    implementation that compares actual datetime values rather than raw text.

    Raw lexicographic TEXT comparison does not satisfy this contract: `'Z'` sorts above `'.'`, so a no-millisecond row
    compares as greater than the same instant written with milliseconds, and is misclassified against any bound with a
    nonzero millisecond component.

    Tests for the interval contract should include mixed millisecond/no-millisecond persisted representations.

19. **Single-active-session invariant** — At most one active session may exist across the whole database, not one per
    game.

    The primary enforcement is the schema: `one_active_session`, a partial unique index on
    `sessions (status) WHERE status = 'active'`, created in `001_initial_schema.sql`. No `PRAGMA` bypasses a unique
    index, so a second active row cannot be persisted through any supported path. **Tests must not drop this index.**

    Add/update/start additionally reject creation of a second active row in the service layer. That service-layer check
    is the contracted, testable behavior: it must reject before reaching the database and must produce a clean rejection
    rather than surfacing a constraint error.

    `restoreActiveSession()` retains the service-layer repair logic — sort active rows newest start first with highest
    ID as the tie-breaker, retain the newest restorable row, interrupt every extra — as defense in depth against a
    database that predates or loses the index. Because the index makes a multi-active state unreachable through
    supported fixtures, that repair path is explicitly **not** required to be covered by tests, and its absence from
    coverage is not a gap. What tests must cover is the single-row restoration path and the orphaned-row path in item
    25.

20. **Session persistence validation** — Every persisted session needs a valid start and nonnegative tracked duration.
    Active sessions have no end. Completed/interrupted sessions require a valid end not earlier than start; equal
    timestamps are valid zero-length sessions. The repository is the final enforcement boundary.

21. **Lifecycle signals** — Emit `sessionStarted` whenever persistence crosses nonexistent/inactive → active, whether
    automatic or manual. Emit `sessionStopped` whenever persistence crosses active → completed/interrupted, including
    restoration repairs. Do not emit for active → active or inactive → inactive edits. Removing an active row is
    rejected; inactive removal emits no lifecycle signal.

22. **Stopped-signal payload** — `sessionStopped` carries `Session` by value and `Session` is a declared Qt metatype.
    Slots may modify their local copy and explicitly persist it; signal delivery is not an in/out mutation mechanism.

23. **Ending an active session** — Replace `trackedDuration` with wall-clock `start.secsTo(end)`; do not add to an
    existing duration. If the current time is invalid or earlier than start, fail without modifying persistence or
    clamping into an invalid timestamp combination.

24. **Automatic detection** — Direct automatic start rejects `trackingEnabled == false`. When several games are
    detected, retain a still-detected pending game; otherwise prefer Steam identity over path-only identity, then choose
    the lower game ID deterministically.

25. **Orphaned active restoration** — Clear stale cached state before restoration.

    `restoreActiveSession()` must handle an orphaned active session if one is found in persisted storage: an active row
    whose game is missing is changed to `Interrupted` with a valid end/duration, emits `sessionStopped`, and restoration
    continues. Restoration fails only if the repair cannot be persisted.

    Under normal operation this state is unreachable, because `sessions.game_id` declares `ON DELETE CASCADE` and
    `DatabaseManager::configureDatabase()` enables `PRAGMA foreign_keys = ON`, so deleting a game deletes its sessions.
    Tests may temporarily disable foreign-key enforcement with `PRAGMA foreign_keys = OFF` to construct this
    otherwise-unreachable fixture.

26. **Session note loading** — Repository queries use a left join and populate `Session.notes` from `session_documents`.
    A missing legacy document yields empty notes rather than hiding the session.

27. **Session document lifecycle** — Every inserted session receives a document row even for empty notes.
    `last_saved_timestamp_utc` changes only when note content changes or a missing document is created; unrelated
    session updates leave it unchanged. A changed note is assigned a timestamp strictly later than the prior saved
    timestamp.

28. **Session/document atomicity** — Insert and update wrap the session row and document operation in one transaction.
    Any document or commit failure rolls back the session change. If insert assigned an ID before rollback, restore the
    caller's `Session.id` to zero.

29. **Corrupted session rows** — `SessionRepository::query` must defensively handle corrupted rows even when those rows
    could not have been produced through the current schema. Unknown source/status, invalid timestamps, negative
    duration, or invalid status/end combinations cause only that row to be logged and skipped. Other valid rows remain
    in the query result.

    Unknown enums and negative durations are blocked by `CHECK` constraints in `001_initial_schema.sql`. Tests may use
    `PRAGMA ignore_check_constraints = ON` to construct such fixtures. Invalid timestamps and invalid status/end
    combinations have no corresponding constraint and can be inserted directly.

30. **Database manager lifecycle** — A successful repeated `initialize()` is idempotently true. Empty/blank paths fail;
    explicit `:memory:` remains valid. Configuration/migration failure closes and removes only the manager-owned
    connection, leaving no half-initialized state and never removing another owner's colliding connection.

31. **Migration ledger compatibility** — A recorded migration is applied only when both version and name match the
    compiled migration. Unknown versions—including versions newer than this binary—fail startup as incompatible. Pending
    migrations still apply transactionally in compiled order.

32. **Legacy artwork migration** — `artwork_path` maps to true only when non-null and nonblank after trimming ASCII
    whitespace. Null, empty, and whitespace-only values map false. The migration is deterministic and does not inspect
    the filesystem.

33. **Process/game identity** — When both process and candidate game have Steam App IDs, the IDs are authoritative. A
    mismatch remains false even when executable paths match. Path matching is allowed when one side lacks usable Steam
    identity.

34. **Steam process cache** — Treat a process's Steam App ID as immutable while its PID/executable-path cache entry
    remains live. Read on new PID or executable-path change; purge absent PIDs. Tests inject the reader rather than
    depending on arbitrary `/proc/<pid>/environ` contents.

35. **Runtime lifecycle** — The fixed `GameLogRuntimeConnection` expresses one live runtime per process. `start()` while
    running fails. The same instance supports `start() → stop() → start()` by recreating its process source and
    restoring service state.

36. **Qt warning policy** — This is a test-suite policy, not a production-code switch. Use `QTest::failOnWarning()`
    selectively in deterministic repository/service tests; explicitly consume expected warnings. Avoid a blanket policy
    in GUI/network/keychain/live-process integration tests where platform warnings may be legitimate.
