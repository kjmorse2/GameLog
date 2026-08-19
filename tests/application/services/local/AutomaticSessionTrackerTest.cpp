#include <QtTest/QtTest>

#include <chrono>
#include <vector>

#include "application/services/local/AutomaticSessionTracker.h"

using gamelog::application::services::AutomaticSessionTracker;
using gamelog::application::services::TrackingAction;
using gamelog::application::services::TrackingDecision;
using gamelog::core::domain::Game;
using gamelog::core::process::ProcessInfo;
using std::chrono::seconds;

namespace
{
    constexpr seconds kGracePeriod{30};

    Game makeGame(const int id, const QString& executablePath, const std::optional<int> steamAppId = std::nullopt)
    {
        Game game;
        game.id = id;
        game.title = QStringLiteral("Game %1").arg(id);
        game.executablePath = executablePath;
        game.steamAppId = steamAppId;
        return game;
    }

    ProcessInfo makeProcess(const QString& executablePath, const std::optional<std::uint32_t> steamAppId = std::nullopt)
    {
        ProcessInfo process;
        process.pid = 1234;
        process.executablePath = executablePath;
        process.steamAppId = steamAppId;
        return process;
    }
} // namespace

/**
 * Exercises the grace-period state machine directly.
 *
 * These tests need no database, repository, GameService, or clock: that is the
 * point of extracting the tracker out of SessionService.
 */
class AutomaticSessionTrackerTest : public QObject
{
    Q_OBJECT

private slots:
    void advance_ignoresNonPositiveElapsed();

    void advance_doesNotStartBeforeTheGracePeriodElapses();

    void advance_startsOnceTheGracePeriodIsReached();

    void advance_restartsTheGracePeriodWhenDetectionFlapsToAnotherGame();

    void advance_restartsTheGracePeriodWhenDetectionIsLost();

    void advance_startsImmediatelyWhenElapsedSkipsPastTheThreshold();

    void advance_doesNotStopWhileTheActiveGameIsStillRunning();

    void advance_stopsOnlyAfterTheActiveGameIsAbsentForTheGracePeriod();

    void advance_resetsTheCloseTimerWhenTheGameReappears();

    void advance_prefersSteamMatchesOverPathMatches();

    void advance_retainsThePendingGameWhileItIsStillDetected();

    void reset_clearsPendingState();
};

void AutomaticSessionTrackerTest::advance_ignoresNonPositiveElapsed()
{
    AutomaticSessionTracker tracker;
    const Game game = makeGame(1, QStringLiteral("/games/a"));
    QHash<QString, Game> paths;
    paths.insert(game.executablePath, game);

    const TrackingDecision decision = tracker.advance({makeProcess(QStringLiteral("/games/a"))},
                                                      seconds::zero(),
                                                      nullptr,
                                                      {},
                                                      paths);

    QCOMPARE(decision.action, TrackingAction::None);
    QVERIFY(!tracker.pendingGameId().has_value());
}

void AutomaticSessionTrackerTest::advance_doesNotStartBeforeTheGracePeriodElapses()
{
    AutomaticSessionTracker tracker;
    const Game game = makeGame(1, QStringLiteral("/games/a"));
    QHash<QString, Game> paths;
    paths.insert(game.executablePath, game);
    const std::vector<ProcessInfo> processes{makeProcess(QStringLiteral("/games/a"))};

    for(seconds elapsed{0}; elapsed < kGracePeriod - seconds{5}; elapsed += seconds{5})
    {
        const TrackingDecision decision = tracker.advance(processes, seconds{5}, nullptr, {}, paths);
        QCOMPARE(decision.action, TrackingAction::None);
    }

    QCOMPARE(tracker.pendingGameId().value_or(0), game.id);
}

void AutomaticSessionTrackerTest::advance_startsOnceTheGracePeriodIsReached()
{
    AutomaticSessionTracker tracker;
    const Game game = makeGame(7, QStringLiteral("/games/a"));
    QHash<QString, Game> paths;
    paths.insert(game.executablePath, game);
    const std::vector<ProcessInfo> processes{makeProcess(QStringLiteral("/games/a"))};

    TrackingDecision decision;
    for(seconds elapsed{0}; elapsed < kGracePeriod; elapsed += seconds{10})
    {
        decision = tracker.advance(processes, seconds{10}, nullptr, {}, paths);
    }

    QCOMPARE(decision.action, TrackingAction::Start);
    QVERIFY(decision.game.has_value());
    QCOMPARE(decision.game->id, game.id);

    // The pending state is cleared once the decision is handed back.
    QVERIFY(!tracker.pendingGameId().has_value());
}

void AutomaticSessionTrackerTest::advance_restartsTheGracePeriodWhenDetectionFlapsToAnotherGame()
{
    AutomaticSessionTracker tracker;
    const Game first = makeGame(1, QStringLiteral("/games/a"));
    const Game second = makeGame(2, QStringLiteral("/games/b"));

    QHash<QString, Game> paths;
    paths.insert(first.executablePath, first);
    paths.insert(second.executablePath, second);

    // 25 of the 30 seconds accumulate against the first game.
    for(int tick = 0; tick < 5; ++tick)
    {
        QCOMPARE(tracker.advance({makeProcess(QStringLiteral("/games/a"))}, seconds{5}, nullptr, {}, paths).action,
                 TrackingAction::None);
    }

    // Switching games must restart the count rather than inherit it.
    const TrackingDecision decision = tracker.advance({makeProcess(QStringLiteral("/games/b"))},
                                                      seconds{5},
                                                      nullptr,
                                                      {},
                                                      paths);
    QCOMPARE(decision.action, TrackingAction::None);
    QCOMPARE(tracker.pendingGameId().value_or(0), second.id);
}

void AutomaticSessionTrackerTest::advance_restartsTheGracePeriodWhenDetectionIsLost()
{
    AutomaticSessionTracker tracker;
    const Game game = makeGame(1, QStringLiteral("/games/a"));
    QHash<QString, Game> paths;
    paths.insert(game.executablePath, game);

    for(int tick = 0; tick < 5; ++tick)
    {
        static_cast<void>(tracker.advance({makeProcess(QStringLiteral("/games/a"))}, seconds{5}, nullptr, {}, paths));
    }

    // An empty snapshot clears the pending candidate entirely.
    QCOMPARE(tracker.advance({}, seconds{5}, nullptr, {}, paths).action, TrackingAction::None);
    QVERIFY(!tracker.pendingGameId().has_value());

    // Detection resuming starts a fresh grace period, so 25s is not enough.
    for(int tick = 0; tick < 5; ++tick)
    {
        QCOMPARE(tracker.advance({makeProcess(QStringLiteral("/games/a"))}, seconds{5}, nullptr, {}, paths).action,
                 TrackingAction::None);
    }
}

void AutomaticSessionTrackerTest::advance_startsImmediatelyWhenElapsedSkipsPastTheThreshold()
{
    AutomaticSessionTracker tracker;
    const Game game = makeGame(1, QStringLiteral("/games/a"));
    QHash<QString, Game> paths;
    paths.insert(game.executablePath, game);

    // A long stall between polls must not lose the start.
    const TrackingDecision decision = tracker.advance({makeProcess(QStringLiteral("/games/a"))},
                                                      kGracePeriod * 10,
                                                      nullptr,
                                                      {},
                                                      paths);

    QCOMPARE(decision.action, TrackingAction::Start);
    QCOMPARE(decision.game->id, game.id);
}

void AutomaticSessionTrackerTest::advance_doesNotStopWhileTheActiveGameIsStillRunning()
{
    AutomaticSessionTracker tracker;
    const Game game = makeGame(1, QStringLiteral("/games/a"));

    for(int tick = 0; tick < 20; ++tick)
    {
        const TrackingDecision decision = tracker.advance({makeProcess(QStringLiteral("/games/a"))},
                                                          seconds{10},
                                                          &game,
                                                          {},
                                                          {});
        QCOMPARE(decision.action, TrackingAction::None);
    }
}

void AutomaticSessionTrackerTest::advance_stopsOnlyAfterTheActiveGameIsAbsentForTheGracePeriod()
{
    AutomaticSessionTracker tracker;
    const Game game = makeGame(1, QStringLiteral("/games/a"));

    QCOMPARE(tracker.advance({}, seconds{10}, &game, {}, {}).action, TrackingAction::None);
    QCOMPARE(tracker.advance({}, seconds{10}, &game, {}, {}).action, TrackingAction::None);

    const TrackingDecision decision = tracker.advance({}, seconds{10}, &game, {}, {});
    QCOMPARE(decision.action, TrackingAction::Stop);
    QVERIFY(!decision.game.has_value());
}

void AutomaticSessionTrackerTest::advance_resetsTheCloseTimerWhenTheGameReappears()
{
    AutomaticSessionTracker tracker;
    const Game game = makeGame(1, QStringLiteral("/games/a"));

    // Nearly closed...
    QCOMPARE(tracker.advance({}, seconds{25}, &game, {}, {}).action, TrackingAction::None);

    // ...then the game is seen again, which must clear the accumulated absence.
    QCOMPARE(tracker.advance({makeProcess(QStringLiteral("/games/a"))}, seconds{5}, &game, {}, {}).action,
             TrackingAction::None);

    // A further 25s is therefore still not enough to stop.
    QCOMPARE(tracker.advance({}, seconds{25}, &game, {}, {}).action, TrackingAction::None);
}

void AutomaticSessionTrackerTest::advance_prefersSteamMatchesOverPathMatches()
{
    AutomaticSessionTracker tracker;
    const Game steamGame = makeGame(9, QStringLiteral("/games/steam"), 600);
    const Game pathGame = makeGame(2, QStringLiteral("/games/path"));

    QHash<std::uint32_t, Game> steam;
    steam.insert(600U, steamGame);
    QHash<QString, Game> paths;
    paths.insert(pathGame.executablePath, pathGame);

    // The path game has the lower ID, so only precedence can explain the winner.
    const std::vector<ProcessInfo> processes{
        makeProcess(QStringLiteral("/games/path")),
        makeProcess(QStringLiteral("/games/steam"), 600U)
    };

    const TrackingDecision decision = tracker.advance(processes, kGracePeriod, nullptr, steam, paths);

    QCOMPARE(decision.action, TrackingAction::Start);
    QCOMPARE(decision.game->id, steamGame.id);
}

void AutomaticSessionTrackerTest::advance_retainsThePendingGameWhileItIsStillDetected()
{
    AutomaticSessionTracker tracker;
    const Game first = makeGame(5, QStringLiteral("/games/a"));
    const Game second = makeGame(1, QStringLiteral("/games/b"));

    QHash<QString, Game> paths;
    paths.insert(first.executablePath, first);
    paths.insert(second.executablePath, second);

    // Game 5 becomes pending on its own.
    static_cast<void>(tracker.advance({makeProcess(QStringLiteral("/games/a"))}, seconds{10}, nullptr, {}, paths));
    QCOMPARE(tracker.pendingGameId().value_or(0), first.id);

    // Game 1 appears alongside it. Despite the lower ID tie-break, the pending
    // game is retained because it is still detected.
    const std::vector<ProcessInfo> both{
        makeProcess(QStringLiteral("/games/a")),
        makeProcess(QStringLiteral("/games/b"))
    };

    const TrackingDecision decision = tracker.advance(both, seconds{20}, nullptr, {}, paths);

    QCOMPARE(decision.action, TrackingAction::Start);
    QCOMPARE(decision.game->id, first.id);
}

void AutomaticSessionTrackerTest::reset_clearsPendingState()
{
    AutomaticSessionTracker tracker;
    const Game game = makeGame(1, QStringLiteral("/games/a"));
    QHash<QString, Game> paths;
    paths.insert(game.executablePath, game);

    static_cast<void>(tracker.advance({makeProcess(QStringLiteral("/games/a"))}, seconds{25}, nullptr, {}, paths));
    QVERIFY(tracker.pendingGameId().has_value());

    tracker.reset();
    QVERIFY(!tracker.pendingGameId().has_value());

    // The cleared timer means 25s of detection no longer completes a start.
    QCOMPARE(tracker.advance({makeProcess(QStringLiteral("/games/a"))}, seconds{25}, nullptr, {}, paths).action,
             TrackingAction::None);
}

QTEST_MAIN(AutomaticSessionTrackerTest)

#include "AutomaticSessionTrackerTest.moc"
