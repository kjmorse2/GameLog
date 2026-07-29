#include <QtTest/QtTest>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "fixtures/TestDatabaseFixture.h"

#include <memory>

using gamelog::core::database::DatabaseManager;
using gamelog::core::database::GameRepository;
using gamelog::core::domain::Game;

class GameRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void findById_returnsNulloptForMissingId();
    void findById_returnsInsertedGame();
    void findAll_returnsGamesOrderedByTitleCaseInsensitive();
    void findAll_returnsEmptyWhenNoRowsExist();
    void insert_persistsAllFieldsAndAssignsId();
    void insert_handlesUnsetOptionalFields();
    void update_persistsModifiedFields();
    void update_returnsTrueForMissingRow();
    void remove_deletesExistingRow();
    void remove_returnsTrueForMissingRow();

private:
    static Game makeGame(const QString &title);

    QString databasePath_;
    std::unique_ptr<DatabaseManager> manager_;
    std::unique_ptr<GameRepository> repository_;
};

void GameRepositoryTest::init()
{
    databasePath_ = gamelog::tests::fixtures::createFreshTestDatabasePath(
            QString{"game-repository-%1"}.arg(QTest::currentTestFunction()));
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

Game GameRepositoryTest::makeGame(const QString &title)
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

void GameRepositoryTest::findById_returnsNulloptForMissingId()
{
    const auto game = repository_->findById(999999);
    QVERIFY(!game.has_value());
}

void GameRepositoryTest::findById_returnsInsertedGame()
{
    Game game = makeGame("Stardew");
    QVERIFY(repository_->insert(game));

    const auto loaded = repository_->findById(game.id);
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

void GameRepositoryTest::findAll_returnsGamesOrderedByTitleCaseInsensitive()
{
    Game zelda = makeGame("zelda");
    Game alpha = makeGame("Alpha");
    Game beta = makeGame("beta");

    QVERIFY(repository_->insert(zelda));
    QVERIFY(repository_->insert(alpha));
    QVERIFY(repository_->insert(beta));

    const auto games = repository_->findAll();
    QCOMPARE(games.size(), 3);
    QCOMPARE(games[0].title, QString("Alpha"));
    QCOMPARE(games[1].title, QString("beta"));
    QCOMPARE(games[2].title, QString("zelda"));
}

void GameRepositoryTest::findAll_returnsEmptyWhenNoRowsExist()
{
    const auto games = repository_->findAll();
    QVERIFY(games.empty());
}

void GameRepositoryTest::insert_persistsAllFieldsAndAssignsId()
{
    Game game = makeGame("Cyberpunk");
    QVERIFY(repository_->insert(game));
    QVERIFY(game.id > 0);

    const auto loaded = repository_->findById(game.id);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->title, QString("Cyberpunk"));
    QVERIFY(loaded->steamAppId.has_value());
    QVERIFY(loaded->artworkPath.has_value());
}

void GameRepositoryTest::insert_handlesUnsetOptionalFields()
{
    Game game = makeGame("Portal");
    game.steamAppId.reset();
    game.artworkPath.reset();

    QVERIFY(repository_->insert(game));
    QVERIFY(game.id > 0);

    const auto loaded = repository_->findById(game.id);
    QVERIFY(loaded.has_value());
    QVERIFY(!loaded->steamAppId.has_value());
    QVERIFY(!loaded->artworkPath.has_value());
}

void GameRepositoryTest::update_persistsModifiedFields()
{
    Game game = makeGame("Factorio");
    QVERIFY(repository_->insert(game));

    game.title = "Factorio 2";
    game.executablePath = "/games/factorio2";
    game.executableName = "factorio2.bin";
    game.steamAppId.reset();
    game.artworkPath.reset();
    game.trackingEnabled = false;

    QVERIFY(repository_->update(game));

    const auto loaded = repository_->findById(game.id);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->title, QString("Factorio 2"));
    QCOMPARE(loaded->executablePath, QString("/games/factorio2"));
    QCOMPARE(loaded->executableName, QString("factorio2.bin"));
    QVERIFY(!loaded->steamAppId.has_value());
    QVERIFY(!loaded->artworkPath.has_value());
    QVERIFY(!loaded->trackingEnabled);
}

void GameRepositoryTest::update_returnsTrueForMissingRow()
{
    Game game = makeGame("Missing");
    game.id = 999999;
    QVERIFY(repository_->update(game));
}

void GameRepositoryTest::remove_deletesExistingRow()
{
    Game game = makeGame("DeleteMe");
    QVERIFY(repository_->insert(game));

    QVERIFY(repository_->remove(game.id));
    const auto loaded = repository_->findById(game.id);
    QVERIFY(!loaded.has_value());
}

void GameRepositoryTest::remove_returnsTrueForMissingRow()
{
    QVERIFY(repository_->remove(999999));
}

QTEST_GUILESS_MAIN(GameRepositoryTest)

#include "GameRepositoryTest.moc"
