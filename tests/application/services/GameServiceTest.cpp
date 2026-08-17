#include <QtTest/QtTest>

#include "../../../src/application/services/local/GameService.h"
#include "database/DatabaseManager.h"
#include "domain/Game.h"
#include "fixtures/TestDatabaseFixture.h"

#include <memory>

namespace gamelog::core::database
{
    class DatabaseManager;
}

using gamelog::core::database::DatabaseManager;
using gamelog::application::services::GameService;
using gamelog::core::domain::Game;
using gamelog::core::database::GameRepository;

namespace
{
    class GameServiceTest :public QObject
    {
        Q_OBJECT

    private
    slots:
        void init();

        void cleanup();

        void search_returnsAllForEmptyQuery();

        void search_returnsNoneForEmptyQueryOnEmptyDatabase();

        void findById_returnsNulloptForMissingId();

        void findById_returnsInsertedGame();

        void findByExecutableName_returnsNulloptForMissingName();

        void findByExecutableName_returnsInsertedGame();

        void listGames_returnsAllGames();

        void listTrackedGames_returnsOnlyTrackedGames();

        void listTrackedGames_returnsGamesOrderedByTitleCaseInsensitive();

        void listTrackedGames_returnsEmptyWhenNoRowsExist();

        void addGame_persistsAllFieldsAndAssignsId();

        void addGame_handlesUnsetOptionalFields();

        void updateGame_persistsModifiedFields();

        void updateGame_returnsTrueForMissingRow();

        void removeGame_deletesExistingRow();

        void removeGame_returnsTrueForMissingRow();

        void syncGamesWithDatabase_rebuildsInMemoryIndexes();

    private:
        static Game makeGame(const QString &title);

        QString databasePath_;
        std::unique_ptr<DatabaseManager> manager_;
        std::unique_ptr<GameRepository> repository_;
        std::unique_ptr<GameService> service_;
    };
}

void GameServiceTest::init()
{
    QTest::failOnWarning();
    databasePath_ = gamelog::tests::fixtures::createFreshTestDatabasePath(QString{"game-repository-%1"}.arg(QTest::currentTestFunction()));
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("game-repository");

    manager_ = std::make_unique<DatabaseManager>(databasePath_, connectionName);
    QVERIFY(manager_->initialize());

    repository_ = std::make_unique<GameRepository>(manager_->database());
    service_ = std::make_unique<GameService>(*repository_);
}

void GameServiceTest::cleanup()
{
    service_.reset();
    repository_.reset();
    manager_.reset();
    gamelog::tests::fixtures::cleanupDatabaseArtifacts(databasePath_);
}

void GameServiceTest::search_returnsAllForEmptyQuery()
{
    Game game1 = makeGame("Game1");
    Game game2 = makeGame("Game2");
    QVERIFY(service_->addGame(game1));
    QVERIFY(service_->addGame(game2));

    const auto games = service_->search({});
    QCOMPARE(games.size(), 2);
}

void GameServiceTest::search_returnsNoneForEmptyQueryOnEmptyDatabase()
{
    const auto games = service_->search({});
    QCOMPARE(games.size(), 0);
}

void GameServiceTest::findById_returnsNulloptForMissingId()
{
    const auto game = service_->findById(999999);
    QVERIFY(!game.has_value());
}

void GameServiceTest::findById_returnsInsertedGame()
{
    Game game = makeGame("Stardew");
    QVERIFY(service_->addGame(game));

    const auto loaded = service_->findById(game.id);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->id, game.id);
    QCOMPARE(loaded->title, game.title);
    QCOMPARE(loaded->executablePath, game.executablePath);
    QCOMPARE(loaded->executableName, game.executableName);
    QVERIFY(loaded->steamAppId.has_value());
    QCOMPARE(*loaded->steamAppId, *game.steamAppId);
    QVERIFY(loaded->artworkPath.has_value());
    QCOMPARE(*loaded->artworkPath, *game.artworkPath);
    QCOMPARE(loaded->trackingEnabled, game.trackingEnabled);
}

void GameServiceTest::findByExecutableName_returnsNulloptForMissingName()
{
    const auto game = service_->findByExecutableName("nonexistent.exe");
    QVERIFY(!game.has_value());
}

void GameServiceTest::findByExecutableName_returnsInsertedGame()
{
    const QString executableName = "hollow_knight.exe";
    Game game = makeGame("Hollow Knight");
    game.executableName = executableName;
    QVERIFY(service_->addGame(game));

    const auto loaded = service_->findByExecutableName(executableName);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->id, game.id);
    QCOMPARE(loaded->title, game.title);
    QCOMPARE(loaded->executablePath, game.executablePath);
    QCOMPARE(loaded->executableName, game.executableName);

}

void GameServiceTest::listGames_returnsAllGames()
{
    Game game1 = makeGame("Tracked");
    Game game2 = makeGame("Untracked");
    game1.trackingEnabled = false;

    QVERIFY(service_->addGame(game1));
    QVERIFY(service_->addGame(game2));

    const auto games = service_->listGames();
    QCOMPARE(games.size(), 2);
}

void GameServiceTest::listTrackedGames_returnsOnlyTrackedGames()
{
    Game trackedGame = makeGame("Tracked");
    Game untrackedGame = makeGame("Untracked");
    untrackedGame.trackingEnabled = false;

    QVERIFY(service_->addGame(trackedGame));
    QVERIFY(service_->addGame(untrackedGame));

    const auto games = service_->listTrackedGames();
    QCOMPARE(games.size(), 1);
    QCOMPARE(games[0].title, QString("Tracked"));
}

void GameServiceTest::listTrackedGames_returnsGamesOrderedByTitleCaseInsensitive()
{
    Game zelda = makeGame("zelda");
    Game alpha = makeGame("Alpha");
    Game beta = makeGame("beta");

    QVERIFY(service_->addGame(zelda));
    QVERIFY(service_->addGame(alpha));
    QVERIFY(service_->addGame(beta));

    const auto games = service_->listTrackedGames();
    QCOMPARE(games.size(), 3);
    QCOMPARE(games[0].title, QString("Alpha"));
    QCOMPARE(games[1].title, QString("beta"));
    QCOMPARE(games[2].title, QString("zelda"));
}

void GameServiceTest::listTrackedGames_returnsEmptyWhenNoRowsExist()
{
    const auto games = service_->listTrackedGames();
    QVERIFY(games.empty());
}

void GameServiceTest::addGame_persistsAllFieldsAndAssignsId()
{
    Game game = makeGame("Cyberpunk");
    QVERIFY(service_->addGame(game));
    QVERIFY(game.id > 0);

    const auto loaded = service_->findById(game.id);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->title, QString("Cyberpunk"));
    QVERIFY(loaded->steamAppId.has_value());
    QVERIFY(loaded->artworkPath.has_value());
}

void GameServiceTest::addGame_handlesUnsetOptionalFields()
{
    Game game = makeGame("Portal");
    game.steamAppId.reset();
    game.artworkPath.reset();

    QVERIFY(service_->addGame(game));
    QVERIFY(game.id > 0);

    const auto loaded = service_->findById(game.id);
    QVERIFY(loaded.has_value());
    QVERIFY(!loaded->steamAppId.has_value());
    QVERIFY(!loaded->artworkPath.has_value());
}

void GameServiceTest::updateGame_persistsModifiedFields()
{
    Game game = makeGame("Factorio");
    QVERIFY(service_->addGame(game));

    game.title = "Factorio 2";
    game.executablePath = "/games/factorio2";
    game.executableName = "factorio2.bin";
    game.steamAppId.reset();
    game.artworkPath.reset();
    game.trackingEnabled = false;

    QVERIFY(service_->updateGame(game));

    const auto loaded = service_->findById(game.id);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->title, QString("Factorio 2"));
    QCOMPARE(loaded->executablePath, QString("/games/factorio2"));
    QCOMPARE(loaded->executableName, QString("factorio2.bin"));
    QVERIFY(!loaded->steamAppId.has_value());
    QVERIFY(!loaded->artworkPath.has_value());
    QVERIFY(!loaded->trackingEnabled);
}

void GameServiceTest::updateGame_returnsTrueForMissingRow()
{
    Game game = makeGame("Missing");
    game.id = 999999;
    QVERIFY(service_->updateGame(game));
}

void GameServiceTest::removeGame_deletesExistingRow()
{
    Game game = makeGame("DeleteMe");
    QVERIFY(service_->addGame(game));

    QVERIFY(service_->removeGame(game.id));
    const auto loaded = service_->findById(game.id);
    QVERIFY(!loaded.has_value());
}

void GameServiceTest::removeGame_returnsTrueForMissingRow()
{
    QVERIFY(service_->removeGame(999999));
}

void GameServiceTest::syncGamesWithDatabase_rebuildsInMemoryIndexes()
{
    Game game1 = makeGame("Game1");
    Game game2 = makeGame("Game2");
    QVERIFY(service_->addGame(game1));
    QVERIFY(service_->addGame(game2));

    Game game3 = makeGame("Game3");
    QVERIFY(repository_->insert(game3));

    QHash<std::uint32_t, Game> steamGamesInMemoryIndexBeforeSync = service_->trackedSteamGames();
    QHash<QString, Game> pathGamesInMemoryIndexBeforeSync = service_->trackedPathGames();
    QCOMPARE(steamGamesInMemoryIndexBeforeSync.size(), 2);
    QCOMPARE(pathGamesInMemoryIndexBeforeSync.size(), 2);

    // Sync with the database to rebuild the in-memory indexes
    service_->syncGamesWithDatabase();

    steamGamesInMemoryIndexBeforeSync = service_->trackedSteamGames();
    pathGamesInMemoryIndexBeforeSync = service_->trackedPathGames();
    QCOMPARE(steamGamesInMemoryIndexBeforeSync.size(), 3);
    QCOMPARE(pathGamesInMemoryIndexBeforeSync.size(), 3);
}

Game GameServiceTest::makeGame(const QString &title)
{
    Game game;
    game.title = title;
    game.executablePath = "/games/" + title.toLower();
    game.executableName = title.toLower() + ".bin";
    game.steamAppId = static_cast<int>(qHash(title) & 0x7FFFFFFF);
    game.artworkPath = "/art/" + title + ".png";
    game.trackingEnabled = true;
    return game;
}

QTEST_GUILESS_MAIN(GameServiceTest)

#include "GameServiceTest.moc"
