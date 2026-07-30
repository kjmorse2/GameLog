#include <QtTest/QtTest>
#include "process/ProcessSource.h"
#include "process/ProcfsProcessSource.h"
#include "process/ProcessInfo.h"

    class ProcessTest : public QObject
{
    Q_OBJECT

private slots:
    void detectsSomeProcesses();
};

void ProcessTest::detectsSomeProcesses()
{
    // Test the detection of some processes
    gamelog::core::process::ProcfsProcessSource source = gamelog::core::process::ProcfsProcessSource();
    std::vector<gamelog::core::process::ProcessInfo> processes = source.listProcesses();
    QVERIFY(!processes.empty());
}


QTEST_APPLESS_MAIN(ProcessTest)
#include "ProcessTest.moc"
