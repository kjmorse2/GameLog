#include <QtTest/QtTest>
#include "domain/Session.h"

namespace
{
    const std::vector<QString> source_StringsReturnAutomatic{"automatic", "Automatic"};

    const std::vector<QString> source_StringsReturnManual{"manual", "Manual"};

    const std::vector<QString> status_StringsReturnActive{"active", "Active"};

    const std::vector<QString> status_StringsReturnCompleted{"completed", "Completed"};

    const std::vector<QString> status_StringsReturnInterrupted{"interrupted", "Interrupted"};
} // namespace
namespace
{
    class SessionTest:public QObject
    {
        Q_OBJECT

    private
    slots:

        static void parsesSourceFromString();

        void parsesStatusFromString();
    };
}

void SessionTest::parsesSourceFromString()
{
    for(const QString& sourceString : source_StringsReturnAutomatic)
    {
        QCOMPARE(gamelog::core::domain::sessionSourceFromString(sourceString), gamelog::core::domain::SessionSource::Automatic);
    }
    for(const QString& sourceString : source_StringsReturnManual)
    {
        QCOMPARE(gamelog::core::domain::sessionSourceFromString(sourceString), gamelog::core::domain::SessionSource::Manual);
    }
}

void SessionTest::parsesStatusFromString()
{
    for(const QString& statusString : status_StringsReturnActive)
    {
        QCOMPARE(gamelog::core::domain::sessionStatusFromString(statusString), gamelog::core::domain::SessionStatus::Active);
    }
    for(const QString& statusString : status_StringsReturnCompleted)
    {
        QCOMPARE(gamelog::core::domain::sessionStatusFromString(statusString), gamelog::core::domain::SessionStatus::Completed);
    }
    for(const QString& statusString : status_StringsReturnInterrupted)
    {
        QCOMPARE(gamelog::core::domain::sessionStatusFromString(statusString), gamelog::core::domain::SessionStatus::Interrupted);
    }
}

QTEST_APPLESS_MAIN(SessionTest)

#include "SessionTest.moc"