#pragma once

#include <QString>

namespace gamelog::tests::fixtures {

    [[nodiscard]] QString createFreshTestDatabasePath(const QString &namePrefix = "gamelog-test-db");

    [[nodiscard]] QString createUniqueConnectionName(const QString &namePrefix = "gamelog-test-connection");

    void cleanupDatabaseArtifacts(const QString &databasePath);

} // namespace gamelog::tests::fixtures
