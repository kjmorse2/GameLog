#include <QtTest/QtTest>

#include "process/ProcessInfo.h"
#include "process/SteamProcessInspector.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <QHash>

using gamelog::core::process::ProcessInfo;
using gamelog::core::process::SteamProcessInspector;

namespace
{
    ProcessInfo makeProcess(const qint64 pid, const QString& executablePath)
    {
        ProcessInfo process;
        process.pid = pid;
        process.executablePath = executablePath;
        process.executableName = executablePath.section(QLatin1Char('/'), -1);
        return process;
    }

    /**
     * Counts reader invocations per PID so cache behavior can be observed
     * through the public injection seam instead of private inspector state.
     */
    class RecordingReader
    {
    public:
        explicit RecordingReader(QHash<qint64, std::uint32_t> appIdsByPid)
            : appIdsByPid_{std::move(appIdsByPid)} {}

        [[nodiscard]] SteamProcessInspector::SteamAppIdReader reader()
        {
            return [this](const qint64 pid) -> std::optional<std::uint32_t>
            {
                callsByPid_[pid] += 1;

                const auto found = appIdsByPid_.constFind(pid);
                if(found == appIdsByPid_.constEnd()) { return std::nullopt; }

                return found.value();
            };
        }

        [[nodiscard]] int callCount(const qint64 pid) const { return callsByPid_.value(pid, 0); }

        [[nodiscard]] int totalCallCount() const
        {
            int total = 0;
            for(const int count : callsByPid_) { total += count; }
            return total;
        }

    private:
        QHash<qint64, std::uint32_t> appIdsByPid_;
        QHash<qint64, int> callsByPid_;
    };
} // namespace

namespace
{
    class SteamProcessInspectorTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        static void annotate_populatesSteamAppIdFromReader();

        static void annotate_leavesSteamAppIdUnsetWhenReaderReturnsNullopt();

        static void annotate_handlesEmptySnapshot();

        static void annotate_handlesNullReader();

        static void annotate_readsOncePerNewPid();

        static void annotate_doesNotRereadWhilePidAndPathStayLive();

        static void annotate_rereadsWhenExecutablePathChangesForSamePid();

        static void annotate_purgesCacheEntriesForAbsentPids();

        static void defaultConstructor_producesUsableInspector();
    };
} // namespace

void SteamProcessInspectorTest::annotate_populatesSteamAppIdFromReader()
{
    RecordingReader reader{{{100, 620U}, {101, 730U}}};
    SteamProcessInspector inspector{reader.reader()};

    std::vector<ProcessInfo> processes{
        makeProcess(100, QStringLiteral("/games/portal2")), makeProcess(101, QStringLiteral("/games/csgo"))
    };

    inspector.annotate(processes);

    QVERIFY(processes[0].steamAppId.has_value());
    QCOMPARE(*processes[0].steamAppId, 620U);
    QVERIFY(processes[1].steamAppId.has_value());
    QCOMPARE(*processes[1].steamAppId, 730U);
}

void SteamProcessInspectorTest::annotate_leavesSteamAppIdUnsetWhenReaderReturnsNullopt()
{
    RecordingReader reader{{}};
    SteamProcessInspector inspector{reader.reader()};

    std::vector<ProcessInfo> processes{makeProcess(100, QStringLiteral("/usr/bin/editor"))};
    inspector.annotate(processes);

    QVERIFY(!processes[0].steamAppId.has_value());
    QCOMPARE(reader.callCount(100), 1);
}

void SteamProcessInspectorTest::annotate_handlesEmptySnapshot()
{
    RecordingReader reader{{{100, 620U}}};
    SteamProcessInspector inspector{reader.reader()};

    std::vector<ProcessInfo> processes;
    inspector.annotate(processes);

    QVERIFY(processes.empty());
    QCOMPARE(reader.totalCallCount(), 0);
}

void SteamProcessInspectorTest::annotate_handlesNullReader()
{
    SteamProcessInspector inspector{SteamProcessInspector::SteamAppIdReader{}};

    std::vector<ProcessInfo> processes{makeProcess(100, QStringLiteral("/games/portal2"))};
    processes[0].steamAppId = 999U;

    inspector.annotate(processes);

    QVERIFY(!processes[0].steamAppId.has_value());
}

void SteamProcessInspectorTest::annotate_readsOncePerNewPid()
{
    RecordingReader reader{{{100, 620U}, {101, 730U}}};
    SteamProcessInspector inspector{reader.reader()};

    std::vector<ProcessInfo> processes{
        makeProcess(100, QStringLiteral("/games/portal2")), makeProcess(101, QStringLiteral("/games/csgo"))
    };

    inspector.annotate(processes);

    QCOMPARE(reader.callCount(100), 1);
    QCOMPARE(reader.callCount(101), 1);
    QCOMPARE(reader.totalCallCount(), 2);
}

void SteamProcessInspectorTest::annotate_doesNotRereadWhilePidAndPathStayLive()
{
    RecordingReader reader{{{100, 620U}}};
    SteamProcessInspector inspector{reader.reader()};

    for(int iteration = 0; iteration < 5; ++iteration)
    {
        std::vector<ProcessInfo> processes{makeProcess(100, QStringLiteral("/games/portal2"))};
        inspector.annotate(processes);

        QVERIFY(processes[0].steamAppId.has_value());
        QCOMPARE(*processes[0].steamAppId, 620U);
    }

    // The Steam App ID is immutable for the cached process lifetime.
    QCOMPARE(reader.callCount(100), 1);
}

void SteamProcessInspectorTest::annotate_rereadsWhenExecutablePathChangesForSamePid()
{
    RecordingReader reader{{{100, 620U}}};
    SteamProcessInspector inspector{reader.reader()};

    std::vector<ProcessInfo> first{makeProcess(100, QStringLiteral("/games/portal2"))};
    inspector.annotate(first);
    QCOMPARE(reader.callCount(100), 1);

    std::vector<ProcessInfo> reused{makeProcess(100, QStringLiteral("/games/other"))};
    inspector.annotate(reused);

    QCOMPARE(reader.callCount(100), 2);
    QVERIFY(reused[0].steamAppId.has_value());
    QCOMPARE(*reused[0].steamAppId, 620U);
}

void SteamProcessInspectorTest::annotate_purgesCacheEntriesForAbsentPids()
{
    RecordingReader reader{{{100, 620U}}};
    SteamProcessInspector inspector{reader.reader()};

    std::vector<ProcessInfo> present{makeProcess(100, QStringLiteral("/games/portal2"))};
    inspector.annotate(present);
    QCOMPARE(reader.callCount(100), 1);

    // The PID disappears from the snapshot, so its cache entry is discarded.
    std::vector<ProcessInfo> absent{makeProcess(200, QStringLiteral("/usr/bin/editor"))};
    inspector.annotate(absent);
    QCOMPARE(reader.callCount(100), 1);

    // Its reappearance must therefore trigger a fresh read.
    std::vector<ProcessInfo> returned{makeProcess(100, QStringLiteral("/games/portal2"))};
    inspector.annotate(returned);
    QCOMPARE(reader.callCount(100), 2);
}

void SteamProcessInspectorTest::defaultConstructor_producesUsableInspector()
{
    // The production constructor reads /proc, so only construction and a
    // no-crash annotate() of an empty snapshot are asserted here.
    SteamProcessInspector inspector;

    std::vector<ProcessInfo> processes;
    inspector.annotate(processes);

    QVERIFY(processes.empty());
}

QTEST_APPLESS_MAIN(SteamProcessInspectorTest)

#include "SteamProcessInspectorTest.moc"
