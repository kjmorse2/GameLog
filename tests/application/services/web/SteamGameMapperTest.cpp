#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "application/services/web/SteamGameMapper.h"

using gamelog::application::services::gamesFromSteamOwnedGames;
using gamelog::core::domain::Game;

namespace
{
    QJsonArray parseArray(const QByteArray& json)
    {
        return QJsonDocument::fromJson(json).array();
    }
} // namespace

/**
 * Covers Steam payload mapping in isolation. No database is involved, which is
 * the reason the mapping was split out of GameService.
 */
class SteamGameMapperTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsWellFormedEntries();

    void skipsNonObjectEntries();

    void skipsNonPositiveAppIds();

    void skipsBlankTitles();

    void preservesTitleWhitespaceItDoesNotReject();

    void returnsEmptyForAnEmptyArray();

    void leavesIdUnsetSoTheRepositoryAssignsIt();
};

void SteamGameMapperTest::mapsWellFormedEntries()
{
    const std::vector<Game> games = gamesFromSteamOwnedGames(
        parseArray(R"([{"appid":10,"name":"Half-Life"},{"appid":20,"name":"Team Fortress"}])"));

    QCOMPARE(games.size(), std::size_t{2});
    QCOMPARE(games[0].title, QStringLiteral("Half-Life"));
    QCOMPARE(games[0].steamAppId.value_or(0), 10);
    QCOMPARE(games[1].title, QStringLiteral("Team Fortress"));
    QCOMPARE(games[1].steamAppId.value_or(0), 20);
}

void SteamGameMapperTest::skipsNonObjectEntries()
{
    // One malformed entry must not discard the rest of the library.
    const std::vector<Game> games = gamesFromSteamOwnedGames(
        parseArray(R"([1, "text", null, {"appid":10,"name":"Half-Life"}])"));

    QCOMPARE(games.size(), std::size_t{1});
    QCOMPARE(games[0].steamAppId.value_or(0), 10);
}

void SteamGameMapperTest::skipsNonPositiveAppIds()
{
    const std::vector<Game> games = gamesFromSteamOwnedGames(
        parseArray(R"([{"appid":0,"name":"Zero"},{"appid":-5,"name":"Negative"},{"name":"Missing"},
                       {"appid":10,"name":"Valid"}])"));

    QCOMPARE(games.size(), std::size_t{1});
    QCOMPARE(games[0].title, QStringLiteral("Valid"));
}

void SteamGameMapperTest::skipsBlankTitles()
{
    const std::vector<Game> games = gamesFromSteamOwnedGames(
        parseArray(R"([{"appid":10,"name":""},{"appid":11,"name":"   "},{"appid":12},
                       {"appid":13,"name":"Valid"}])"));

    QCOMPARE(games.size(), std::size_t{1});
    QCOMPARE(games[0].steamAppId.value_or(0), 13);
}

void SteamGameMapperTest::preservesTitleWhitespaceItDoesNotReject()
{
    // Trimming decides acceptance, but the stored title is left as Steam sent it.
    const std::vector<Game> games = gamesFromSteamOwnedGames(parseArray(R"([{"appid":10,"name":" Portal "}])"));

    QCOMPARE(games.size(), std::size_t{1});
    QCOMPARE(games[0].title, QStringLiteral(" Portal "));
}

void SteamGameMapperTest::returnsEmptyForAnEmptyArray()
{
    QVERIFY(gamesFromSteamOwnedGames(QJsonArray{}).empty());
}

void SteamGameMapperTest::leavesIdUnsetSoTheRepositoryAssignsIt()
{
    const std::vector<Game> games = gamesFromSteamOwnedGames(parseArray(R"([{"appid":10,"name":"Half-Life"}])"));

    QCOMPARE(games.size(), std::size_t{1});
    QCOMPARE(games[0].id, 0);
    QVERIFY(games[0].executablePath.isEmpty());
    QVERIFY(games[0].trackingEnabled);
}

QTEST_MAIN(SteamGameMapperTest)

#include "SteamGameMapperTest.moc"
