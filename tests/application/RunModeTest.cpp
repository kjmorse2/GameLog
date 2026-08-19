#include <QtTest/QtTest>

#include "application/RunMode.h"

#include <array>
#include <optional>
#include <vector>

using gamelog::application::RunMode;
using gamelog::application::determineRunMode;

namespace
{
    /**
     * Builds a mutable argv-style array from the supplied arguments. determineRunMode
     * takes char* argv[] to match main(), so the backing strings must be non-const.
     */
    class ArgumentVector
    {
    public:
        explicit ArgumentVector(const std::vector<QByteArray>& arguments)
        {
            storage_.reserve(arguments.size());
            pointers_.reserve(arguments.size() + 1);

            for(const QByteArray& argument : arguments) { storage_.push_back(argument); }
            for(QByteArray& argument : storage_) { pointers_.push_back(argument.data()); }

            pointers_.push_back(nullptr);
        }

        [[nodiscard]] int argc() const { return static_cast<int>(storage_.size()); }

        [[nodiscard]] char** argv() { return pointers_.data(); }

    private:
        std::vector<QByteArray> storage_;
        std::vector<char*> pointers_;
    };

    const std::vector<QByteArray> rejectedSingleArguments{
        "--headless=true", "-headless", "headless", "--HEADLESS", "--Gui", "--live ", " --live", "", "--", "--unknown"
    };
} // namespace

namespace
{
    class RunModeTest : public QObject
    {
        Q_OBJECT

    private
        slots  :

        static void determineRunMode_returnsHeadlessForHeadlessArgument();

        static void determineRunMode_returnsGuiForGuiArgument();

        static void determineRunMode_returnsLiveForLiveArgument();

        static void determineRunMode_returnsNulloptWithoutArguments();

        static void determineRunMode_returnsNulloptForRepeatedRunMode();

        static void determineRunMode_returnsNulloptForMixedRunModes();

        static void determineRunMode_returnsNulloptForValidModeWithExtraArgument();

        static void determineRunMode_returnsNulloptForUnrecognizedArguments();

        static void determineRunMode_returnsNulloptForNullArgv();

        static void determineRunMode_returnsNulloptForNullArgumentPointer();
    };
}

void RunModeTest::determineRunMode_returnsHeadlessForHeadlessArgument()
{
    ArgumentVector arguments{{"gamelog", "--headless"}};
    const std::optional<RunMode> mode = determineRunMode(arguments.argc(), arguments.argv());

    QVERIFY(mode.has_value());
    QVERIFY(*mode == RunMode::Headless);
}

void RunModeTest::determineRunMode_returnsGuiForGuiArgument()
{
    ArgumentVector arguments{{"gamelog", "--gui"}};
    const std::optional<RunMode> mode = determineRunMode(arguments.argc(), arguments.argv());

    QVERIFY(mode.has_value());
    QVERIFY(*mode == RunMode::Gui);
}

void RunModeTest::determineRunMode_returnsLiveForLiveArgument()
{
    ArgumentVector arguments{{"gamelog", "--live"}};
    const std::optional<RunMode> mode = determineRunMode(arguments.argc(), arguments.argv());

    QVERIFY(mode.has_value());
    QVERIFY(*mode == RunMode::Live);
}

void RunModeTest::determineRunMode_returnsNulloptWithoutArguments()
{
    ArgumentVector arguments{{"gamelog"}};
    QVERIFY(!determineRunMode(arguments.argc(), arguments.argv()).has_value());
}

void RunModeTest::determineRunMode_returnsNulloptForRepeatedRunMode()
{
    ArgumentVector arguments{{"gamelog", "--gui", "--gui"}};
    QVERIFY(!determineRunMode(arguments.argc(), arguments.argv()).has_value());
}

void RunModeTest::determineRunMode_returnsNulloptForMixedRunModes()
{
    ArgumentVector headlessAndGui{{"gamelog", "--headless", "--gui"}};
    QVERIFY(!determineRunMode(headlessAndGui.argc(), headlessAndGui.argv()).has_value());

    ArgumentVector guiAndLive{{"gamelog", "--gui", "--live"}};
    QVERIFY(!determineRunMode(guiAndLive.argc(), guiAndLive.argv()).has_value());
}

void RunModeTest::determineRunMode_returnsNulloptForValidModeWithExtraArgument()
{
    ArgumentVector trailingExtra{{"gamelog", "--live", "--verbose"}};
    QVERIFY(!determineRunMode(trailingExtra.argc(), trailingExtra.argv()).has_value());

    ArgumentVector leadingExtra{{"gamelog", "--verbose", "--live"}};
    QVERIFY(!determineRunMode(leadingExtra.argc(), leadingExtra.argv()).has_value());
}

void RunModeTest::determineRunMode_returnsNulloptForUnrecognizedArguments()
{
    for(const QByteArray& argument : rejectedSingleArguments)
    {
        ArgumentVector arguments{{"gamelog", argument}};
        QVERIFY2(!determineRunMode(arguments.argc(), arguments.argv()).has_value(), argument.constData());
    }
}

void RunModeTest::determineRunMode_returnsNulloptForNullArgv() { QVERIFY(!determineRunMode(2, nullptr).has_value()); }

void RunModeTest::determineRunMode_returnsNulloptForNullArgumentPointer()
{
    std::array<char*, 2> argv{const_cast<char*>("gamelog"), nullptr};
    QVERIFY(!determineRunMode(2, argv.data()).has_value());
}

QTEST_APPLESS_MAIN(RunModeTest)

#include "RunModeTest.moc"
