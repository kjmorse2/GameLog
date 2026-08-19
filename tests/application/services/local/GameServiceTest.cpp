#include <QtTest/QtTest>

#include "application/services/local/GameService.h"
#include "database/DatabaseManager.h"
#include "domain/Game.h"
#include "fixtures/TestDatabaseFixture.h"

#include <memory>
#include <application/services/local/CredentialService.h>
#include <application/services/web/SteamApiService.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSignalSpy>

namespace gamelog::core::database
{
    class DatabaseManager;
}

using gamelog::core::database::DatabaseManager;
using gamelog::application::services::GameService;
using gamelog::core::domain::Game;
using gamelog::core::database::GameRepository;
using gamelog::application::services::CredentialService;
using gamelog::application::services::SteamApiService;

namespace
{
    /**
     * Keeps every test keychain-free. QKeychain::Job::start() is not virtual, so
     * returning no job at all is the only way to stop CredentialService from
     * contacting the real keychain when GameService delegates a Steam sync.
     */
    class KeychainFreeCredentialService : public CredentialService
    {
    protected:
        QKeychain::WritePasswordJob* createWritePasswordJob() override { return nullptr; }

        QKeychain::ReadPasswordJob* createReadPasswordJob() override
        {
            ++readJobRequests_;
            return nullptr;
        }

        QKeychain::DeletePasswordJob* createDeletePasswordJob() override { return nullptr; }

    public:
        [[nodiscard]] int readJobRequests() const noexcept { return readJobRequests_; }

    private:
        int readJobRequests_{0};
    };

    QJsonObject makeSteamGame(const int appId, const QString& name)
    {
        QJsonObject object;
        object.insert(QStringLiteral("appid"), appId);
        object.insert(QStringLiteral("name"), name);
        return object;
    }
} // namespace

namespace
{
    class GameServiceTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
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

        void updateGame_returnsFalseForMissingRow();

        void removeGame_deletesExistingRow();

        void removeGame_returnsFalseForMissingRow();

        void syncGamesWithDatabase_rebuildsInMemoryIndexes();

        void findByExecutablePath_returnsInsertedGame();

        void findByExecutablePath_returnsNulloptForMissingPath();

        void trackedSteamGames_containsOnlyTrackedGamesWithPositiveSteamIds();

        void trackedPathGames_containsOnlyTrackedGamesWithAnExecutablePath();

        void hasTrackedSteamGames_reflectsTheSteamIndex();

        void setHasArtwork_persistsAvailability();

        void setHasArtwork_isANoOpWhenUnchanged();

        void setHasArtwork_returnsFalseForMissingGame();

        void syncSteamGames_delegatesToTheSteamApiService();

        void onSteamGamesReceived_insertsOnlyUnknownSteamAppIds();

        void onSteamGamesReceived_leavesUntrackedAndLocallyTitledRowsUnchanged();

        void onSteamGamesReceived_skipsMalformedEntriesAndNonPositiveAppIds();

        void syncGamesWithDatabase_keepsLastEntryForDuplicateExecutablePaths();

    private:
        static Game makeGame(const QString& title);

        QString databasePath_;
        std::unique_ptr<DatabaseManager> manager_;
        std::unique_ptr<KeychainFreeCredentialService> credentialService_;
        std::unique_ptr<SteamApiService> steamApiService_;
        std::unique_ptr<GameRepository> repository_;
        std::unique_ptr<GameService> service_;
    };
}

void GameServiceTest::init()
{
    QTest::failOnWarning();
    databasePath_ =
        gamelog::tests::fixtures::createFreshTestDatabasePath(QString{"game-repository-%1"}.
                                                              arg(QTest::currentTestFunction()));
    const QString connectionName = gamelog::tests::fixtures::createUniqueConnectionName("game-repository");

    manager_ = std::make_unique<DatabaseManager>(databasePath_, connectionName);
    QVERIFY(manager_->initialize());

    // GameService stores a SteamApiService reference, so both collaborators must
    // outlive it rather than being init()-scoped temporaries.
    credentialService_ = std::make_unique<KeychainFreeCredentialService>();
    steamApiService_ = std::make_unique<SteamApiService>(*credentialService_);
    repository_ = std::make_unique<GameRepository>(manager_->database());
    service_ = std::make_unique<GameService>(*repository_, *steamApiService_);
}

void GameServiceTest::cleanup()
{
    service_.reset();
    repository_.reset();
    steamApiService_.reset();
    credentialService_.reset();
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
    QVERIFY(!loaded->hasArtwork);
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
}

void GameServiceTest::addGame_handlesUnsetOptionalFields()
{
    Game game = makeGame("Portal");
    game.steamAppId.reset();

    QVERIFY(service_->addGame(game));
    QVERIFY(game.id > 0);

    const auto loaded = service_->findById(game.id);
    QVERIFY(loaded.has_value());
    QVERIFY(!loaded->steamAppId.has_value());
}

void GameServiceTest::updateGame_persistsModifiedFields()
{
    Game game = makeGame("Factorio");
    QVERIFY(service_->addGame(game));

    game.title = "Factorio 2";
    game.executablePath = "/games/factorio2";
    game.executableName = "factorio2.bin";
    game.steamAppId.reset();
    game.trackingEnabled = false;

    QVERIFY(service_->updateGame(game));

    const auto loaded = service_->findById(game.id);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->title, QString("Factorio 2"));
    QCOMPARE(loaded->executablePath, QString("/games/factorio2"));
    QCOMPARE(loaded->executableName, QString("factorio2.bin"));
    QVERIFY(!loaded->steamAppId.has_value());
    QVERIFY(!loaded->trackingEnabled);
}

void GameServiceTest::updateGame_returnsFalseForMissingRow()
{
    Game game = makeGame("Missing");
    // game.id = 999999;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Refusing to update a game without a valid ID:.*"));

    QVERIFY(!service_->updateGame(game));
}

void GameServiceTest::removeGame_deletesExistingRow()
{
    Game game = makeGame("DeleteMe");
    QVERIFY(service_->addGame(game));

    QVERIFY(service_->removeGame(game.id));
    const auto loaded = service_->findById(game.id);
    QVERIFY(!loaded.has_value());
}

void GameServiceTest::removeGame_returnsFalseForMissingRow() { QVERIFY(!service_->removeGame(999999)); }

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

void GameServiceTest::findByExecutablePath_returnsInsertedGame()
{
    Game game = makeGame("Dead Cells");
    QVERIFY(service_->addGame(game));

    const auto loaded = service_->findByExecutablePath(game.executablePath);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->id, game.id);
    QCOMPARE(loaded->executablePath, game.executablePath);
}

void GameServiceTest::findByExecutablePath_returnsNulloptForMissingPath()
{
    Game game = makeGame("Dead Cells");
    QVERIFY(service_->addGame(game));

    QVERIFY(!service_->findByExecutablePath("/games/does-not-exist").has_value());
    QVERIFY(!service_->findByExecutablePath(QString{}).has_value());
}

void GameServiceTest::trackedSteamGames_containsOnlyTrackedGamesWithPositiveSteamIds()
{
    Game steamGame = makeGame("Steam Tracked");
    Game untrackedSteamGame = makeGame("Steam Untracked");
    untrackedSteamGame.trackingEnabled = false;

    Game pathOnlyGame = makeGame("Path Only");
    pathOnlyGame.steamAppId.reset();

    QVERIFY(service_->addGame(steamGame));
    QVERIFY(service_->addGame(untrackedSteamGame));
    QVERIFY(service_->addGame(pathOnlyGame));

    const QHash<std::uint32_t, Game>& index = service_->trackedSteamGames();
    QCOMPARE(index.size(), 1);
    QVERIFY(index.contains(static_cast<std::uint32_t>(*steamGame.steamAppId)));
    QCOMPARE(index.value(static_cast<std::uint32_t>(*steamGame.steamAppId)).id, steamGame.id);
}

void GameServiceTest::trackedPathGames_containsOnlyTrackedGamesWithAnExecutablePath()
{
    Game pathGame = makeGame("With Path");

    Game withoutPath = makeGame("Without Path");
    withoutPath.executablePath.clear();

    Game untracked = makeGame("Untracked");
    untracked.trackingEnabled = false;

    QVERIFY(service_->addGame(pathGame));
    QVERIFY(service_->addGame(withoutPath));
    QVERIFY(service_->addGame(untracked));

    const QHash<QString, Game>& index = service_->trackedPathGames();
    QCOMPARE(index.size(), 1);
    QVERIFY(index.contains(pathGame.executablePath));
    QCOMPARE(index.value(pathGame.executablePath).id, pathGame.id);
}

void GameServiceTest::hasTrackedSteamGames_reflectsTheSteamIndex()
{
    QVERIFY(!service_->hasTrackedSteamGames());

    Game pathOnlyGame = makeGame("Path Only");
    pathOnlyGame.steamAppId.reset();
    QVERIFY(service_->addGame(pathOnlyGame));
    QVERIFY(!service_->hasTrackedSteamGames());

    Game steamGame = makeGame("Steam Game");
    QVERIFY(service_->addGame(steamGame));
    QVERIFY(service_->hasTrackedSteamGames());

    QVERIFY(service_->removeGame(steamGame.id));
    QVERIFY(!service_->hasTrackedSteamGames());
}

void GameServiceTest::setHasArtwork_persistsAvailability()
{
    Game game = makeGame("Artwork Game");
    QVERIFY(service_->addGame(game));
    QVERIFY(!game.hasArtwork);

    QVERIFY(service_->setHasArtwork(game.id, true));
    QVERIFY(service_->findById(game.id)->hasArtwork);

    QVERIFY(service_->setHasArtwork(game.id, false));
    QVERIFY(!service_->findById(game.id)->hasArtwork);
}

void GameServiceTest::setHasArtwork_isANoOpWhenUnchanged()
{
    Game game = makeGame("Artwork Game");
    QVERIFY(service_->addGame(game));

    const QSignalSpy updatedSpy{service_.get(), &GameService::gameUpdated};
    QVERIFY(updatedSpy.isValid());

    // The stored value already matches, so no write and no update signal occur.
    QVERIFY(service_->setHasArtwork(game.id, false));
    QCOMPARE(updatedSpy.count(), 0);

    QVERIFY(service_->setHasArtwork(game.id, true));
    QCOMPARE(updatedSpy.count(), 1);

    QVERIFY(service_->setHasArtwork(game.id, true));
    QCOMPARE(updatedSpy.count(), 1);
}

void GameServiceTest::setHasArtwork_returnsFalseForMissingGame()
{
    QVERIFY(!service_->setHasArtwork(999999, true));
    QVERIFY(!service_->setHasArtwork(0, true));
    QVERIFY(!service_->setHasArtwork(-1, false));
}

void GameServiceTest::syncSteamGames_delegatesToTheSteamApiService()
{
    const QSignalSpy failedSpy{steamApiService_.get(), &SteamApiService::requestFailed};
    QVERIFY(failedSpy.isValid());

    // Job creation is stubbed out, so the delegated request fails with a
    // credential error rather than reaching a keychain.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Steam API request failed.*"));

    service_->syncSteamGames();

    // Delegation is observable through the Steam service asking CredentialService
    // for both secrets; job creation is stubbed out, so nothing reaches a keychain.
    QCOMPARE(credentialService_->readJobRequests(), 2);
    QCOMPARE(failedSpy.count(), 1);
}

void GameServiceTest::onSteamGamesReceived_insertsOnlyUnknownSteamAppIds()
{
    Game existing = makeGame("Existing");
    existing.steamAppId = 100;
    QVERIFY(service_->addGame(existing));

    QJsonArray steamGames;
    steamGames.append(makeSteamGame(100, "Steam Title For Existing"));
    steamGames.append(makeSteamGame(300, "Brand New Game"));

    emit
    steamApiService_->ownedGamesReceived(steamGames);

    QCOMPARE(service_->listGames().size(), 2);

    const auto untouched = service_->findById(existing.id);
    QVERIFY(untouched.has_value());
    QCOMPARE(untouched->title, QString("Existing"));

    GameQuery newGameQuery;
    newGameQuery.steamAppId = 300;
    const auto inserted = service_->search(newGameQuery);
    QCOMPARE(inserted.size(), 1);
    QCOMPARE(inserted[0].title, QString("Brand New Game"));
}

void GameServiceTest::onSteamGamesReceived_leavesUntrackedAndLocallyTitledRowsUnchanged()
{
    Game untracked = makeGame("My Own Title");
    untracked.steamAppId = 200;
    untracked.trackingEnabled = false;
    QVERIFY(service_->addGame(untracked));

    // The untracked row is not in the in-memory index, so synchronization must
    // search the whole database to find it.
    QVERIFY(!service_->trackedSteamGames().contains(200U));

    QJsonArray steamGames;
    steamGames.append(makeSteamGame(200, "Steam Supplied Title"));

    emit
    steamApiService_->ownedGamesReceived(steamGames);

    QCOMPARE(service_->listGames().size(), 1);

    const auto reloaded = service_->findById(untracked.id);
    QVERIFY(reloaded.has_value());
    QCOMPARE(reloaded->title, QString("My Own Title"));
    QVERIFY(!reloaded->trackingEnabled);
    QCOMPARE(reloaded->executablePath, untracked.executablePath);
    QCOMPARE(reloaded->executableName, untracked.executableName);
    QCOMPARE(*reloaded->steamAppId, 200);
}

void GameServiceTest::onSteamGamesReceived_skipsMalformedEntriesAndNonPositiveAppIds()
{
    QJsonArray steamGames;
    steamGames.append(QJsonValue{42});
    steamGames.append(QJsonValue{QStringLiteral("not an object")});
    steamGames.append(QJsonValue{QJsonArray{}});
    steamGames.append(makeSteamGame(0, "Zero App Id"));
    steamGames.append(makeSteamGame(-5, "Negative App Id"));
    steamGames.append(makeSteamGame(400, "   "));
    steamGames.append(makeSteamGame(401, ""));
    steamGames.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("No App Id")}});
    steamGames.append(QJsonObject{{QStringLiteral("appid"), 402}});
    steamGames.append(makeSteamGame(500, "Valid Entry"));

    emit
    steamApiService_->ownedGamesReceived(steamGames);

    const auto games = service_->listGames();
    QCOMPARE(games.size(), 1);
    QCOMPARE(games[0].title, QString("Valid Entry"));
    QCOMPARE(*games[0].steamAppId, 500);
}

void GameServiceTest::syncGamesWithDatabase_keepsLastEntryForDuplicateExecutablePaths()
{
    // Executable paths are not unique in persistence, so the index keeps the
    // last row written for a path. listTrackedGames() orders by title, making
    // the alphabetically last title the surviving entry.
    Game first = makeGame("Alpha Launcher");
    first.executablePath = "/games/shared/launcher";

    Game second = makeGame("Zeta Launcher");
    second.executablePath = "/games/shared/launcher";

    QVERIFY(service_->addGame(first));
    QVERIFY(service_->addGame(second));

    service_->syncGamesWithDatabase();

    const QHash<QString, Game>& index = service_->trackedPathGames();
    QCOMPARE(index.size(), 1);
    QCOMPARE(index.value("/games/shared/launcher").id, second.id);
    QCOMPARE(index.value("/games/shared/launcher").title, QString("Zeta Launcher"));

    // Both rows still exist in persistence; only the index collapses them.
    QCOMPARE(service_->listGames().size(), 2);
}

Game GameServiceTest::makeGame(const QString& title)
{
    Game game;
    game.title = title;
    game.executablePath = "/games/" + title.toLower();
    game.executableName = title.toLower() + ".bin";
    game.steamAppId = static_cast<int>(qHash(title) & 0x7FFFFFFF);
    game.trackingEnabled = true;
    return game;
}

QTEST_GUILESS_MAIN(GameServiceTest)

#include "GameServiceTest.moc"
