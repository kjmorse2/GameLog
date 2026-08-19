#include <QtTest/QtTest>
#include "domain/Session.h"

#include <stdexcept>
#include <vector>

namespace
{
    const std::vector<QString> source_StringsReturnAutomatic{"automatic", "Automatic"};

    const std::vector<QString> source_StringsReturnManual{"manual", "Manual"};

    const std::vector<QString> status_StringsReturnActive{"active", "Active"};

    const std::vector<QString> status_StringsReturnCompleted{"completed", "Completed"};

    const std::vector<QString> status_StringsReturnInterrupted{"interrupted", "Interrupted"};

    const std::vector<QString> rejected_SourceStrings{
        "", " ", "   ", "\t", " automatic", "automatic ", " Manual ", "AUTOMATIC", "MANUAL", "aUtomatic", "manuel",
        "active", "unknown"
    };

    const std::vector<QString> rejected_StatusStrings{
        "", " ", "   ", "\t", " active", "active ", " Completed ", "ACTIVE", "COMPLETED", "INTERRUPTED", "aCtive",
        "complete", "automatic", "unknown"
    };
} // namespace
namespace
{
    class SessionTest : public QObject
    {
        Q_OBJECT

    private
        slots  :

        static void parsesSourceFromString();

        void parsesStatusFromString();

        static void sessionSourceFromString_throwsForUnsupportedSpellings();

        static void sessionStatusFromString_throwsForUnsupportedSpellings();

        static void streamsSessionSourceToDebug();

        static void streamsSessionStatusToDebug();

        static void streamsSessionToDebug();
    };
}

void SessionTest::parsesSourceFromString()
{
    for(const QString& sourceString : source_StringsReturnAutomatic)
    {
        QCOMPARE(gamelog::core::domain::sessionSourceFromString(sourceString),
                 gamelog::core::domain::SessionSource::Automatic);
    }
    for(const QString& sourceString : source_StringsReturnManual)
    {
        QCOMPARE(gamelog::core::domain::sessionSourceFromString(sourceString),
                 gamelog::core::domain::SessionSource::Manual);
    }
}

void SessionTest::parsesStatusFromString()
{
    for(const QString& statusString : status_StringsReturnActive)
    {
        QCOMPARE(gamelog::core::domain::sessionStatusFromString(statusString),
                 gamelog::core::domain::SessionStatus::Active);
    }
    for(const QString& statusString : status_StringsReturnCompleted)
    {
        QCOMPARE(gamelog::core::domain::sessionStatusFromString(statusString),
                 gamelog::core::domain::SessionStatus::Completed);
    }
    for(const QString& statusString : status_StringsReturnInterrupted)
    {
        QCOMPARE(gamelog::core::domain::sessionStatusFromString(statusString),
                 gamelog::core::domain::SessionStatus::Interrupted);
    }
}

void SessionTest::sessionSourceFromString_throwsForUnsupportedSpellings()
{
    // Parsing preserves the exact accepted spellings: no trimming and no
    // general case-insensitivity.
    for(const QString& sourceString : rejected_SourceStrings)
    {
        bool threw = false;

        try { static_cast<void>(gamelog::core::domain::sessionSourceFromString(sourceString)); }
        catch(const std::invalid_argument&) { threw = true; }

        QVERIFY2(threw, qPrintable(QStringLiteral("accepted unsupported source: '%1'").arg(sourceString)));
    }
}

void SessionTest::sessionStatusFromString_throwsForUnsupportedSpellings()
{
    for(const QString& statusString : rejected_StatusStrings)
    {
        bool threw = false;

        try { static_cast<void>(gamelog::core::domain::sessionStatusFromString(statusString)); }
        catch(const std::invalid_argument&) { threw = true; }

        QVERIFY2(threw, qPrintable(QStringLiteral("accepted unsupported status: '%1'").arg(statusString)));
    }
}

void SessionTest::streamsSessionSourceToDebug()
{
    QString output;
    QDebug stream{&output};

    stream << gamelog::core::domain::SessionSource::Automatic << gamelog::core::domain::SessionSource::Manual;

    QVERIFY(!output.trimmed().isEmpty());
}

void SessionTest::streamsSessionStatusToDebug()
{
    QString output;
    QDebug stream{&output};

    stream << gamelog::core::domain::SessionStatus::Active << gamelog::core::domain::SessionStatus::Completed <<
        gamelog::core::domain::SessionStatus::Interrupted;

    QVERIFY(!output.trimmed().isEmpty());
}

void SessionTest::streamsSessionToDebug()
{
    gamelog::core::domain::Session session;
    session.id = 42;
    session.gameId = 7;
    session.startTimestamp = QDateTime::currentDateTimeUtc();

    QString output;
    QDebug stream{&output};
    stream << session;

    QVERIFY(output.contains(QStringLiteral("42")));
}

QTEST_APPLESS_MAIN(SessionTest)

#include "SessionTest.moc"
