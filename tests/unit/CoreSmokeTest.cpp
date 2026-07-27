#include <QtTest/QtTest>

#include "domain/Game.h"
#include "domain/Session.h"

class CoreSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void gameDefaultsAreSensible();
    void sessionDefaultsAreSensible();
};

void CoreSmokeTest::gameDefaultsAreSensible()
{
    const gamelog::core::domain::Game game;

    QCOMPARE(game.id, 0);
    QVERIFY(game.title.isEmpty());
    QVERIFY(game.trackingEnabled);
    QVERIFY(!game.steamAppId.has_value());
}

void CoreSmokeTest::sessionDefaultsAreSensible()
{
    const gamelog::core::domain::Session session;

    QCOMPARE(session.status, gamelog::core::domain::SessionStatus::Active);
    QCOMPARE(session.source, gamelog::core::domain::SessionSource::Automatic);
    QVERIFY(!session.endTimestamp.has_value());
}

QTEST_APPLESS_MAIN(CoreSmokeTest)

#include "CoreSmokeTest.moc"
