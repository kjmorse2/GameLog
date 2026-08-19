#pragma once

namespace gamelog::tests::fixtures
{
    /**
     * Force-enables every gamelog.* logging category for the current process.
     *
     * Tests that use QTest::ignoreMessage() assert on messages the production
     * code logs. Those messages are only produced when the category is enabled,
     * so an ambient QT_LOGGING_RULES or qtlogging.ini that silences logging makes
     * such tests fail with "Not all expected messages were received" even though
     * the behavior under test is correct.
     *
     * Rules from the environment are applied after, and therefore override,
     * QLoggingCategory::setFilterRules(). An installed category filter is the
     * only mechanism that outranks both, so that is what this installs. Call it
     * from initTestCase(); it is idempotent and affects only the test process.
     */
    void enableGameLogLoggingCategories();
} // namespace gamelog::tests::fixtures
