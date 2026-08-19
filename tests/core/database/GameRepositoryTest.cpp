#include <QtTest/QtTest>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "domain/Game.h"
#include "domain/query/GameQuery.h"
#include "fixtures/TestDatabaseFixture.h"

#include <cstddef>
#include <memory>

#include <QSqlQuery>
#include <QVariant>

using gamelog::core::database::DatabaseManager;
using gamelog::core::database::GameRepository;
using gamelog::core::domain::Game;
using gamelog::core::domain::query::GameQuery;
using gamelog::core::domain::query::GameSortField;
using gamelog::core::domain::query::SortDirection;

namespace
{
    /**
     * Builds a fully populated game so persistence round-trips can assert on
     * every column rather than only the required ones.
     */
    Game makeGame(const QString& title, const std::optional<int> steamAppId = std::nullopt)
    {
        Game game;
        game.title = title;
        game.executablePath = QStringLiteral("/games/") + title.toLower();
        game.executableName = title.toLower() + QStringLiteral(".bin");
        game.steamAppId = steamAppId;
        game.hasArtwork = false;
        game.trackingEnabled = true;
        return game;
    }

    /**
     * Reads one row of PRAGMA table_info for the supplied column.
     */
    bool columnIsNotNullWithDefault(const QSqlDatabase& database,
                                    const QString& table,
                                    const QString& column,
                                    const QString& expectedDefault)
    {
        QSqlQuery query{database};

        if(!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) { return false; }

        while(query.next())
        {
            if(query.value(QStringLiteral("name")).toString() != column) { continue; }

            return query.value(QStringLiteral("notnull")).toInt() == 1 && query.value(QStringLiteral("dflt_value")).
                   toString() == expectedDefault;
        }

        return false;
    }

    const std::vector<QString> blankTitles{
        QStringLiteral(""), QStringLiteral(" "), QStringLiteral("   "), QStringLiteral("\t"), QStringLiteral("\n"),
        QStringLiteral(" \t\n ")
    };
} // namespace

namespace
{
    class GameRepositoryTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        void cleanup();

        void insert_assignsIdAndPersistsEveryField();

        void insert_persistsUnsetOptionalFields();

        void query_returnsEmptyVectorForEmptyTable();

        void query_roundTripsPersistedRow();

        void query_filtersByEveryPredicate();

        void query_matchesTitleCaseInsensitively();

        void query_appliesLimitOffsetAndCombination();

        void query_appliesSortFieldAndDirection();

        void update_persistsModifiedFields();

        void update_returnsFalseForMissingRow();

        void remove_deletesExistingRow();

        void remove_returnsFalseForZeroNegativeAndMissingIds();

        void insert_rejectsPreassignedId();

        void update_rejectsNonPositiveId();

        void insert_rejectsBlankTitle();

        void update_rejectsBlankTitle();

        void insert_rejectsNonPositiveSteamAppId();

        void update_rejectsNonPositiveSteamAppId();

        void hasArtwork_isNonNullAndDefaultsFalse();

        void insert_rejectsDuplicateSteamAppIdWithoutMerging();

        void insert_permitsDuplicateExecutableNameAndPath();

    private:
        QString databasePath_;
        std::unique_ptr<DatabaseManager> manager_;
        std::unique_ptr<GameRepository> repository_;
    };
} // namespace

void GameRepositoryTest::init()
{
    QTest::failOnWarning();

    databasePath_ =
        gamelog::tests::fixtures::createFreshTestDatabasePath(QStringLiteral("game-repository-%1").
                                                              arg(QString::fromLatin1(QTest::currentTestFunction())));
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("game-repository");

    manager_ = std::make_unique<DatabaseManager>(databasePath_, connectionName);
    QVERIFY(manager_->initialize());

    repository_ = std::make_unique<GameRepository>(manager_->database());
}

void GameRepositoryTest::cleanup()
{
    repository_.reset();
    manager_.reset();
    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath_);
}

void GameRepositoryTest::insert_assignsIdAndPersistsEveryField()
{
    Game game = makeGame(QStringLiteral("Celeste"), 504230);
    game.hasArtwork = true;
    game.trackingEnabled = false;

    QVERIFY(repository_->insert(game));
    QVERIFY(game.id > 0);

    GameQuery query;
    query.ids = {game.id};

    const std::vector<Game> games = repository_->query(query);
    QCOMPARE(static_cast<int>(games.size()), 1);
    QCOMPARE(games[0].id, game.id);
    QCOMPARE(games[0].title, QStringLiteral("Celeste"));
    QCOMPARE(games[0].executablePath, QStringLiteral("/games/celeste"));
    QCOMPARE(games[0].executableName, QStringLiteral("celeste.bin"));
    QVERIFY(games[0].steamAppId.has_value());
    QCOMPARE(*games[0].steamAppId, 504230);
    QVERIFY(games[0].hasArtwork);
    QVERIFY(!games[0].trackingEnabled);
}

void GameRepositoryTest::insert_persistsUnsetOptionalFields()
{
    Game game;
    game.title = QStringLiteral("Bare Minimum");

    QVERIFY(repository_->insert(game));
    QVERIFY(game.id > 0);

    GameQuery query;
    query.ids = {game.id};

    const std::vector<Game> games = repository_->query(query);
    QCOMPARE(static_cast<int>(games.size()), 1);
    QVERIFY(!games[0].steamAppId.has_value());
    QVERIFY(games[0].executablePath.isEmpty());
    QVERIFY(games[0].executableName.isEmpty());
    QVERIFY(!games[0].hasArtwork);
    QVERIFY(games[0].trackingEnabled);
}

void GameRepositoryTest::query_returnsEmptyVectorForEmptyTable()
{
    QVERIFY(repository_->query({}).empty());

    GameQuery filtered;
    filtered.title = QStringLiteral("Nothing");
    QVERIFY(repository_->query(filtered).empty());
}

void GameRepositoryTest::query_roundTripsPersistedRow()
{
    Game game = makeGame(QStringLiteral("Hades"), 1145360);
    QVERIFY(repository_->insert(game));

    const std::vector<Game> games = repository_->query({});
    QCOMPARE(static_cast<int>(games.size()), 1);
    QCOMPARE(games[0].id, game.id);
    QCOMPARE(games[0].title, game.title);
    QCOMPARE(games[0].executableName, game.executableName);
}

void GameRepositoryTest::query_filtersByEveryPredicate()
{
    Game steamGame = makeGame(QStringLiteral("Steam Game"), 111);
    Game pathGame = makeGame(QStringLiteral("Path Game"));
    pathGame.trackingEnabled = false;

    QVERIFY(repository_->insert(steamGame));
    QVERIFY(repository_->insert(pathGame));

    GameQuery byId;
    byId.ids = {steamGame.id};
    QCOMPARE(static_cast<int>(repository_->query(byId).size()), 1);

    GameQuery byBothIds;
    byBothIds.ids = {steamGame.id, pathGame.id};
    QCOMPARE(static_cast<int>(repository_->query(byBothIds).size()), 2);

    GameQuery byTitle;
    byTitle.title = QStringLiteral("Path Game");
    QCOMPARE(static_cast<int>(repository_->query(byTitle).size()), 1);

    GameQuery byExecutableName;
    byExecutableName.executableName = QStringLiteral("steam game.bin");
    QCOMPARE(static_cast<int>(repository_->query(byExecutableName).size()), 1);

    GameQuery byExecutablePath;
    byExecutablePath.executablePath = QStringLiteral("/games/steam game");
    QCOMPARE(static_cast<int>(repository_->query(byExecutablePath).size()), 1);

    GameQuery bySteamAppId;
    bySteamAppId.steamAppId = 111;
    QCOMPARE(static_cast<int>(repository_->query(bySteamAppId).size()), 1);

    GameQuery byTracking;
    byTracking.trackingEnabled = true;
    const std::vector<Game> tracked = repository_->query(byTracking);
    QCOMPARE(static_cast<int>(tracked.size()), 1);
    QCOMPARE(tracked[0].id, steamGame.id);

    GameQuery combined;
    combined.title = QStringLiteral("Steam Game");
    combined.steamAppId = 999;
    QVERIFY(repository_->query(combined).empty());
}

void GameRepositoryTest::query_matchesTitleCaseInsensitively()
{
    Game game = makeGame(QStringLiteral("Portal"));
    QVERIFY(repository_->insert(game));

    GameQuery query;
    query.title = QStringLiteral("pOrTaL");

    const std::vector<Game> games = repository_->query(query);
    QCOMPARE(static_cast<int>(games.size()), 1);
    QCOMPARE(games[0].id, game.id);
}

void GameRepositoryTest::query_appliesLimitOffsetAndCombination()
{
    Game alpha = makeGame(QStringLiteral("Alpha"));
    Game beta = makeGame(QStringLiteral("Beta"));
    Game gamma = makeGame(QStringLiteral("Gamma"));

    QVERIFY(repository_->insert(alpha));
    QVERIFY(repository_->insert(beta));
    QVERIFY(repository_->insert(gamma));

    GameQuery zeroLimit;
    zeroLimit.limit = std::size_t{0};
    QVERIFY(repository_->query(zeroLimit).empty());

    GameQuery singleLimit;
    singleLimit.limit = std::size_t{1};
    const std::vector<Game> firstOnly = repository_->query(singleLimit);
    QCOMPARE(static_cast<int>(firstOnly.size()), 1);
    QCOMPARE(firstOnly[0].title, QStringLiteral("Alpha"));

    GameQuery offsetOnly;
    offsetOnly.offset = std::size_t{1};
    const std::vector<Game> skipped = repository_->query(offsetOnly);
    QCOMPARE(static_cast<int>(skipped.size()), 2);
    QCOMPARE(skipped[0].title, QStringLiteral("Beta"));

    GameQuery offsetPastEnd;
    offsetPastEnd.offset = std::size_t{10};
    QVERIFY(repository_->query(offsetPastEnd).empty());

    GameQuery limitAndOffset;
    limitAndOffset.limit = std::size_t{1};
    limitAndOffset.offset = std::size_t{1};
    const std::vector<Game> middle = repository_->query(limitAndOffset);
    QCOMPARE(static_cast<int>(middle.size()), 1);
    QCOMPARE(middle[0].title, QStringLiteral("Beta"));

    GameQuery limitBeyondSize;
    limitBeyondSize.limit = std::size_t{100};
    QCOMPARE(static_cast<int>(repository_->query(limitBeyondSize).size()), 3);
}

void GameRepositoryTest::query_appliesSortFieldAndDirection()
{
    Game zelda = makeGame(QStringLiteral("zelda"));
    Game alpha = makeGame(QStringLiteral("Alpha"));
    Game beta = makeGame(QStringLiteral("beta"));

    QVERIFY(repository_->insert(zelda));
    QVERIFY(repository_->insert(alpha));
    QVERIFY(repository_->insert(beta));

    GameQuery titleAscending;
    titleAscending.sortBy = GameSortField::Title;
    titleAscending.sortDirection = SortDirection::Ascending;

    const std::vector<Game> byTitle = repository_->query(titleAscending);
    QCOMPARE(static_cast<int>(byTitle.size()), 3);
    QCOMPARE(byTitle[0].title, QStringLiteral("Alpha"));
    QCOMPARE(byTitle[1].title, QStringLiteral("beta"));
    QCOMPARE(byTitle[2].title, QStringLiteral("zelda"));

    GameQuery titleDescending;
    titleDescending.sortBy = GameSortField::Title;
    titleDescending.sortDirection = SortDirection::Descending;

    const std::vector<Game> byTitleDescending = repository_->query(titleDescending);
    QCOMPARE(byTitleDescending[0].title, QStringLiteral("zelda"));
    QCOMPARE(byTitleDescending[2].title, QStringLiteral("Alpha"));

    GameQuery idAscending;
    idAscending.sortBy = GameSortField::Id;
    idAscending.sortDirection = SortDirection::Ascending;

    const std::vector<Game> byId = repository_->query(idAscending);
    QCOMPARE(byId[0].id, zelda.id);
    QCOMPARE(byId[2].id, beta.id);

    GameQuery idDescending;
    idDescending.sortBy = GameSortField::Id;
    idDescending.sortDirection = SortDirection::Descending;

    const std::vector<Game> byIdDescending = repository_->query(idDescending);
    QCOMPARE(byIdDescending[0].id, beta.id);
    QCOMPARE(byIdDescending[2].id, zelda.id);
}

void GameRepositoryTest::update_persistsModifiedFields()
{
    Game game = makeGame(QStringLiteral("Factorio"), 427520);
    QVERIFY(repository_->insert(game));

    game.title = QStringLiteral("Factorio 2");
    game.executablePath = QStringLiteral("/games/factorio2");
    game.executableName = QStringLiteral("factorio2.bin");
    game.steamAppId.reset();
    game.hasArtwork = true;
    game.trackingEnabled = false;

    QVERIFY(repository_->update(game));

    GameQuery query;
    query.ids = {game.id};

    const std::vector<Game> games = repository_->query(query);
    QCOMPARE(static_cast<int>(games.size()), 1);
    QCOMPARE(games[0].title, QStringLiteral("Factorio 2"));
    QCOMPARE(games[0].executablePath, QStringLiteral("/games/factorio2"));
    QCOMPARE(games[0].executableName, QStringLiteral("factorio2.bin"));
    QVERIFY(!games[0].steamAppId.has_value());
    QVERIFY(games[0].hasArtwork);
    QVERIFY(!games[0].trackingEnabled);
}

void GameRepositoryTest::update_returnsFalseForMissingRow()
{
    Game game = makeGame(QStringLiteral("Ghost"));
    game.id = 999999;

    QVERIFY(!repository_->update(game));
}

void GameRepositoryTest::remove_deletesExistingRow()
{
    Game game = makeGame(QStringLiteral("Delete Me"));
    QVERIFY(repository_->insert(game));

    QVERIFY(repository_->remove(game.id));
    QVERIFY(repository_->query({}).empty());
}

void GameRepositoryTest::remove_returnsFalseForZeroNegativeAndMissingIds()
{
    QVERIFY(!repository_->remove(0));
    QVERIFY(!repository_->remove(-1));
    QVERIFY(!repository_->remove(999999));
}

void GameRepositoryTest::insert_rejectsPreassignedId()
{
    Game game = makeGame(QStringLiteral("Preassigned"));
    game.id = 7;

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Refusing to insert a game with a preassigned ID.*"));
    QVERIFY(!repository_->insert(game));
    QCOMPARE(game.id, 7);
    QVERIFY(repository_->query({}).empty());
}

void GameRepositoryTest::update_rejectsNonPositiveId()
{
    Game game = makeGame(QStringLiteral("No Identity"));

    for(const int id : {0, -1, -999})
    {
        game.id = id;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Refusing to update a game without a valid ID.*"));
        QVERIFY(!repository_->update(game));
    }
}

void GameRepositoryTest::insert_rejectsBlankTitle()
{
    for(const QString& title : blankTitles)
    {
        Game game = makeGame(QStringLiteral("Placeholder"));
        game.title = title;

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*empty or whitespace-only title.*"));
        QVERIFY(!repository_->insert(game));
        QCOMPARE(game.id, 0);
    }

    QVERIFY(repository_->query({}).empty());
}

void GameRepositoryTest::update_rejectsBlankTitle()
{
    Game game = makeGame(QStringLiteral("Valid Title"));
    QVERIFY(repository_->insert(game));

    Game blank = game;
    blank.title = QStringLiteral("   ");

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*empty or whitespace-only title.*"));
    QVERIFY(!repository_->update(blank));

    GameQuery query;
    query.ids = {game.id};
    QCOMPARE(repository_->query(query)[0].title, QStringLiteral("Valid Title"));
}

void GameRepositoryTest::insert_rejectsNonPositiveSteamAppId()
{
    for(const int steamAppId : {0, -1})
    {
        Game game = makeGame(QStringLiteral("Bad Steam Id"), steamAppId);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*non-positive Steam App ID.*"));
        QVERIFY(!repository_->insert(game));
        QCOMPARE(game.id, 0);
    }
}

void GameRepositoryTest::update_rejectsNonPositiveSteamAppId()
{
    Game game = makeGame(QStringLiteral("Good Steam Id"), 42);
    QVERIFY(repository_->insert(game));

    Game invalid = game;
    invalid.steamAppId = 0;

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*non-positive Steam App ID.*"));
    QVERIFY(!repository_->update(invalid));

    GameQuery query;
    query.ids = {game.id};
    QCOMPARE(*repository_->query(query)[0].steamAppId, 42);
}

void GameRepositoryTest::hasArtwork_isNonNullAndDefaultsFalse()
{
    QVERIFY(columnIsNotNullWithDefault(manager_->database(),
                                       QStringLiteral("games"),
                                       QStringLiteral("has_artwork"),
                                       QStringLiteral("0")));

    // A row written without touching has_artwork still reads back as a
    // non-null false rather than as a null variant.
    QSqlQuery insertQuery{manager_->database()};
    QVERIFY(insertQuery.exec(QStringLiteral("INSERT INTO games (title) VALUES ('Schema Default')")));

    const std::vector<Game> games = repository_->query({});
    QCOMPARE(static_cast<int>(games.size()), 1);
    QVERIFY(!games[0].hasArtwork);

    QSqlQuery rawQuery{manager_->database()};
    QVERIFY(rawQuery.exec(QStringLiteral("SELECT has_artwork FROM games")));
    QVERIFY(rawQuery.next());
    QVERIFY(!rawQuery.value(0).isNull());
    QCOMPARE(rawQuery.value(0).toInt(), 0);
}

void GameRepositoryTest::insert_rejectsDuplicateSteamAppIdWithoutMerging()
{
    Game original = makeGame(QStringLiteral("Original Title"), 620);
    QVERIFY(repository_->insert(original));

    Game duplicate = makeGame(QStringLiteral("Steam Supplied Title"), 620);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Failed to insert game.*"));
    QVERIFY(!repository_->insert(duplicate));
    QCOMPARE(duplicate.id, 0);

    // The failure must not have merged or updated the existing row.
    const std::vector<Game> games = repository_->query({});
    QCOMPARE(static_cast<int>(games.size()), 1);
    QCOMPARE(games[0].id, original.id);
    QCOMPARE(games[0].title, QStringLiteral("Original Title"));
}

void GameRepositoryTest::insert_permitsDuplicateExecutableNameAndPath()
{
    Game first = makeGame(QStringLiteral("Shared Launcher A"));
    first.executablePath = QStringLiteral("/games/shared/launcher");
    first.executableName = QStringLiteral("launcher");

    Game second = makeGame(QStringLiteral("Shared Launcher B"));
    second.executablePath = first.executablePath;
    second.executableName = first.executableName;

    QVERIFY(repository_->insert(first));
    QVERIFY(repository_->insert(second));
    QVERIFY(first.id != second.id);

    GameQuery query;
    query.executablePath = first.executablePath;
    QCOMPARE(static_cast<int>(repository_->query(query).size()), 2);
}

QTEST_GUILESS_MAIN(GameRepositoryTest)

#include "GameRepositoryTest.moc"
