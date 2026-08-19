#include <QtTest/QtTest>

#include "domain/Game.h"
#include "process/ProcessHelpers.h"
#include "process/ProcessInfo.h"

#include <cstdint>
#include <limits>
#include <optional>

#include <QHash>

using gamelog::core::domain::Game;
using gamelog::core::process::MatchKind;
using gamelog::core::process::ProcessHelpers;
using gamelog::core::process::TrackedGameMatch;
using gamelog::core::process::ProcessInfo;

namespace
{
    Game makeGame(const int id, const QString& executablePath, const std::optional<int> steamAppId = std::nullopt)
    {
        Game game;
        game.id = id;
        game.title = QStringLiteral("Game %1").arg(id);
        game.executablePath = executablePath;
        game.executableName = executablePath.section(QLatin1Char('/'), -1);
        game.steamAppId = steamAppId;
        return game;
    }

    ProcessInfo makeProcess(const qint64 pid,
                            const QString& executablePath,
                            const std::optional<std::uint32_t> steamAppId = std::nullopt)
    {
        ProcessInfo process;
        process.pid = pid;
        process.executablePath = executablePath;
        process.executableName = executablePath.section(QLatin1Char('/'), -1);
        process.steamAppId = steamAppId;
        return process;
    }
} // namespace

namespace
{
    class ProcessHelpersTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        static void processMatchesGame_matchesOnSteamAppId();

        static void processMatchesGame_matchesOnExecutablePathWithoutSteamIdentity();

        static void processMatchesGame_returnsFalseForDifferentExecutablePaths();

        static void processMatchesGame_returnsFalseForEmptyGameExecutablePath();

        static void processMatchesGame_fallsBackToPathWhenGameSteamIdIsNonPositive();

        static void processMatchesGame_fallsBackToPathWhenProcessHasNoSteamId();

        static void processMatchesGame_steamAppIdsAreAuthoritativeOverMatchingPaths();

        static void matchTrackedGame_returnsSteamEntryForSteamProcess();

        static void matchTrackedGame_returnsPathEntryWhenSteamIdentityIsAbsent();

        static void matchTrackedGame_prefersSteamIndexOverPathIndex();

        static void matchTrackedGame_returnsNullptrForEmptyIndexes();

        static void matchTrackedGame_returnsNullptrForEmptyExecutablePath();

        static void matchTrackedGame_returnsNullptrWhenPathEntryFailsIdentityCheck();

        static void readProcessEnvironmentValue_returnsNulloptForNonPositivePid();

        static void readProcessEnvironmentValue_returnsNulloptForEmptyVariableName();

        static void readSteamAppId_returnsNulloptForNonPositivePid();

        static void readSteamAppId_returnsNulloptForUnreadableProcess();
    };
} // namespace

void ProcessHelpersTest::processMatchesGame_matchesOnSteamAppId()
{
    const Game game = makeGame(1, QStringLiteral("/games/one"), 600);
    const ProcessInfo process = makeProcess(100, QStringLiteral("/somewhere/else"), 600U);

    QVERIFY(ProcessHelpers::processMatchesGame(process, game));
}

void ProcessHelpersTest::processMatchesGame_matchesOnExecutablePathWithoutSteamIdentity()
{
    const Game game = makeGame(1, QStringLiteral("/games/one"));
    const ProcessInfo process = makeProcess(100, QStringLiteral("/games/one"));

    QVERIFY(ProcessHelpers::processMatchesGame(process, game));
}

void ProcessHelpersTest::processMatchesGame_returnsFalseForDifferentExecutablePaths()
{
    const Game game = makeGame(1, QStringLiteral("/games/one"));
    const ProcessInfo process = makeProcess(100, QStringLiteral("/games/two"));

    QVERIFY(!ProcessHelpers::processMatchesGame(process, game));
}

void ProcessHelpersTest::processMatchesGame_returnsFalseForEmptyGameExecutablePath()
{
    const Game game = makeGame(1, QString{});
    const ProcessInfo process = makeProcess(100, QString{});

    // Two empty paths must not be treated as a match.
    QVERIFY(!ProcessHelpers::processMatchesGame(process, game));
}

void ProcessHelpersTest::processMatchesGame_fallsBackToPathWhenGameSteamIdIsNonPositive()
{
    for(const int steamAppId : {0, -1})
    {
        const Game game = makeGame(1, QStringLiteral("/games/one"), steamAppId);
        const ProcessInfo matchingPath = makeProcess(100, QStringLiteral("/games/one"), 600U);
        const ProcessInfo otherPath = makeProcess(101, QStringLiteral("/games/two"), 600U);

        QVERIFY(ProcessHelpers::processMatchesGame(matchingPath, game));
        QVERIFY(!ProcessHelpers::processMatchesGame(otherPath, game));
    }
}

void ProcessHelpersTest::processMatchesGame_fallsBackToPathWhenProcessHasNoSteamId()
{
    const Game game = makeGame(1, QStringLiteral("/games/one"), 600);
    const ProcessInfo process = makeProcess(100, QStringLiteral("/games/one"));

    QVERIFY(ProcessHelpers::processMatchesGame(process, game));
}

void ProcessHelpersTest::processMatchesGame_steamAppIdsAreAuthoritativeOverMatchingPaths()
{
    const Game game = makeGame(1, QStringLiteral("/games/shared"), 600);
    const ProcessInfo process = makeProcess(100, QStringLiteral("/games/shared"), 601U);

    // Identical executable paths must not rescue a Steam App ID mismatch.
    QVERIFY(!ProcessHelpers::processMatchesGame(process, game));
}

void ProcessHelpersTest::matchTrackedGame_returnsSteamEntryForSteamProcess()
{
    const Game steamGame = makeGame(1, QStringLiteral("/games/steam"), 600);

    QHash<std::uint32_t, Game> trackedSteamGames;
    trackedSteamGames.insert(600U, steamGame);
    const QHash<QString, Game> trackedPathGames;

    const ProcessInfo process = makeProcess(100, QStringLiteral("/anywhere"), 600U);
    const TrackedGameMatch matched = ProcessHelpers::matchTrackedGame(process, trackedSteamGames, trackedPathGames);

    QVERIFY(matched.game != nullptr);
    QCOMPARE(matched.game->id, steamGame.id);
    QCOMPARE(matched.kind, MatchKind::SteamAppId);
}

void ProcessHelpersTest::matchTrackedGame_returnsPathEntryWhenSteamIdentityIsAbsent()
{
    const Game pathGame = makeGame(2, QStringLiteral("/games/path"));

    const QHash<std::uint32_t, Game> trackedSteamGames;
    QHash<QString, Game> trackedPathGames;
    trackedPathGames.insert(pathGame.executablePath, pathGame);

    const ProcessInfo process = makeProcess(100, QStringLiteral("/games/path"));
    const TrackedGameMatch matched = ProcessHelpers::matchTrackedGame(process, trackedSteamGames, trackedPathGames);

    QVERIFY(matched.game != nullptr);
    QCOMPARE(matched.game->id, pathGame.id);
    QCOMPARE(matched.kind, MatchKind::ExecutablePath);
}

void ProcessHelpersTest::matchTrackedGame_prefersSteamIndexOverPathIndex()
{
    const Game steamGame = makeGame(1, QStringLiteral("/games/shared"), 600);
    const Game pathGame = makeGame(2, QStringLiteral("/games/shared"));

    QHash<std::uint32_t, Game> trackedSteamGames;
    trackedSteamGames.insert(600U, steamGame);

    QHash<QString, Game> trackedPathGames;
    trackedPathGames.insert(pathGame.executablePath, pathGame);

    const ProcessInfo process = makeProcess(100, QStringLiteral("/games/shared"), 600U);
    const TrackedGameMatch matched = ProcessHelpers::matchTrackedGame(process, trackedSteamGames, trackedPathGames);

    QVERIFY(matched.game != nullptr);
    QCOMPARE(matched.game->id, steamGame.id);
    QCOMPARE(matched.kind, MatchKind::SteamAppId);
}

void ProcessHelpersTest::matchTrackedGame_returnsNullptrForEmptyIndexes()
{
    const QHash<std::uint32_t, Game> trackedSteamGames;
    const QHash<QString, Game> trackedPathGames;

    const ProcessInfo process = makeProcess(100, QStringLiteral("/games/one"), 600U);

    const TrackedGameMatch matched = ProcessHelpers::matchTrackedGame(process, trackedSteamGames, trackedPathGames);

    QCOMPARE(matched.game, static_cast<const Game*>(nullptr));
    QCOMPARE(matched.kind, MatchKind::None);
}

void ProcessHelpersTest::matchTrackedGame_returnsNullptrForEmptyExecutablePath()
{
    const Game pathGame = makeGame(2, QString{});

    const QHash<std::uint32_t, Game> trackedSteamGames;
    QHash<QString, Game> trackedPathGames;
    trackedPathGames.insert(QString{}, pathGame);

    const ProcessInfo process = makeProcess(100, QString{});

    const TrackedGameMatch matched = ProcessHelpers::matchTrackedGame(process, trackedSteamGames, trackedPathGames);

    QCOMPARE(matched.game, static_cast<const Game*>(nullptr));
    QCOMPARE(matched.kind, MatchKind::None);
}

void ProcessHelpersTest::matchTrackedGame_returnsNullptrWhenPathEntryFailsIdentityCheck()
{
    // The path index hit is discarded because both sides carry conflicting
    // Steam App IDs, and the Steam index has no entry for the process.
    const Game pathGame = makeGame(2, QStringLiteral("/games/shared"), 600);

    const QHash<std::uint32_t, Game> trackedSteamGames;
    QHash<QString, Game> trackedPathGames;
    trackedPathGames.insert(pathGame.executablePath, pathGame);

    const ProcessInfo process = makeProcess(100, QStringLiteral("/games/shared"), 601U);

    const TrackedGameMatch matched = ProcessHelpers::matchTrackedGame(process, trackedSteamGames, trackedPathGames);

    QCOMPARE(matched.game, static_cast<const Game*>(nullptr));
    QCOMPARE(matched.kind, MatchKind::None);
}

void ProcessHelpersTest::readProcessEnvironmentValue_returnsNulloptForNonPositivePid()
{
    QVERIFY(!ProcessHelpers::readProcessEnvironmentValue(0, QByteArrayLiteral("SteamAppId")).has_value());
    QVERIFY(!ProcessHelpers::readProcessEnvironmentValue(-1, QByteArrayLiteral("SteamAppId")).has_value());
}

void ProcessHelpersTest::readProcessEnvironmentValue_returnsNulloptForEmptyVariableName()
{
    QVERIFY(!ProcessHelpers::readProcessEnvironmentValue(1, QByteArray{}).has_value());
}

void ProcessHelpersTest::readSteamAppId_returnsNulloptForNonPositivePid()
{
    QVERIFY(!ProcessHelpers::readSteamAppId(0).has_value());
    QVERIFY(!ProcessHelpers::readSteamAppId(-1).has_value());
}

void ProcessHelpersTest::readSteamAppId_returnsNulloptForUnreadableProcess()
{
    // A PID far above the configured maximum cannot exist, so /proc lookup fails.
    QVERIFY(!ProcessHelpers::readSteamAppId(std::numeric_limits<qint32>::max()).has_value());
}

QTEST_APPLESS_MAIN(ProcessHelpersTest)

#include "ProcessHelpersTest.moc"
