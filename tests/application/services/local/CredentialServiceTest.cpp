#include <QtTest/QtTest>

#include "application/services/local/CredentialService.h"

#include <memory>
#include <vector>

#include <QSignalSpy>
#include <QString>

using gamelog::application::services::CredentialService;

namespace
{
    const std::vector<QString> blankKeys{
        QStringLiteral(""), QStringLiteral(" "), QStringLiteral("   "), QStringLiteral("\t"), QStringLiteral("\n"),
        QStringLiteral(" \t\n ")
    };

    const std::vector<QString> blankSecrets{
        QStringLiteral(""), QStringLiteral(" "), QStringLiteral("    "), QStringLiteral("\t\n")
    };

    /**
     * Drives the "unable to create job" branches and proves that rejected
     * requests never reach the keychain at all.
     *
     * QKeychain::Job::start() is not virtual, so a fake job returned from these
     * seams would still contact the real keychain. Returning nullptr is the only
     * keychain-free way to exercise the remaining production paths.
     */
    class NullJobCredentialService : public CredentialService
    {
    public:
        [[nodiscard]] int writeJobRequests() const noexcept { return writeJobRequests_; }

        [[nodiscard]] int readJobRequests() const noexcept { return readJobRequests_; }

        [[nodiscard]] int deleteJobRequests() const noexcept { return deleteJobRequests_; }

        [[nodiscard]] int totalJobRequests() const noexcept
        {
            return writeJobRequests_ + readJobRequests_ + deleteJobRequests_;
        }

    protected:
        QKeychain::WritePasswordJob* createWritePasswordJob() override
        {
            ++writeJobRequests_;
            return nullptr;
        }

        QKeychain::ReadPasswordJob* createReadPasswordJob() override
        {
            ++readJobRequests_;
            return nullptr;
        }

        QKeychain::DeletePasswordJob* createDeletePasswordJob() override
        {
            ++deleteJobRequests_;
            return nullptr;
        }

    private:
        int writeJobRequests_{0};
        int readJobRequests_{0};
        int deleteJobRequests_{0};
    };
} // namespace

namespace
{
    class CredentialServiceTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        void cleanup();

        void setSecret_rejectsBlankKeys();

        void getSecret_rejectsBlankKeys();

        void removeSecret_rejectsBlankKeys();

        void setSecret_rejectsBlankSecrets();

        void setSecret_doesNotRemoveExistingValueForBlankSecret();

        void blankRequests_neverReachTheKeychain();

        void setSecret_reportsErrorWhenWriteJobCannotBeCreated();

        void getSecret_reportsErrorWhenReadJobCannotBeCreated();

        void removeSecret_reportsErrorWhenDeleteJobCannotBeCreated();

        static void steamCredentialKeys_areDistinctAndNonBlank();

    private:
        std::unique_ptr<NullJobCredentialService> service_;
        std::unique_ptr<QSignalSpy> errorSpy_;
        std::unique_ptr<QSignalSpy> storedSpy_;
        std::unique_ptr<QSignalSpy> removedSpy_;
        std::unique_ptr<QSignalSpy> retrievedSpy_;
        std::unique_ptr<QSignalSpy> notFoundSpy_;
    };
} // namespace

void CredentialServiceTest::init()
{
    service_ = std::make_unique<NullJobCredentialService>();

    errorSpy_ = std::make_unique<QSignalSpy>(service_.get(), &CredentialService::credentialError);
    storedSpy_ = std::make_unique<QSignalSpy>(service_.get(), &CredentialService::secretStored);
    removedSpy_ = std::make_unique<QSignalSpy>(service_.get(), &CredentialService::secretRemoved);
    retrievedSpy_ = std::make_unique<QSignalSpy>(service_.get(), &CredentialService::secretRetrieved);
    notFoundSpy_ = std::make_unique<QSignalSpy>(service_.get(), &CredentialService::secretNotFound);

    QVERIFY(errorSpy_->isValid());
    QVERIFY(storedSpy_->isValid());
    QVERIFY(removedSpy_->isValid());
    QVERIFY(retrievedSpy_->isValid());
    QVERIFY(notFoundSpy_->isValid());
}

void CredentialServiceTest::cleanup()
{
    notFoundSpy_.reset();
    retrievedSpy_.reset();
    removedSpy_.reset();
    storedSpy_.reset();
    errorSpy_.reset();
    service_.reset();
}

void CredentialServiceTest::setSecret_rejectsBlankKeys()
{
    for(const QString& key : blankKeys)
    {
        errorSpy_->clear();
        service_->setSecret(key, QStringLiteral("a-real-secret"));

        QCOMPARE(errorSpy_->count(), 1);
        QCOMPARE(errorSpy_->at(0).at(0).toString(), key);
        QVERIFY(errorSpy_->at(0).at(1).toString().contains(QStringLiteral("key")));
    }

    QCOMPARE(storedSpy_->count(), 0);
    QCOMPARE(removedSpy_->count(), 0);
}

void CredentialServiceTest::getSecret_rejectsBlankKeys()
{
    for(const QString& key : blankKeys)
    {
        errorSpy_->clear();
        service_->getSecret(key);

        QCOMPARE(errorSpy_->count(), 1);
        QCOMPARE(errorSpy_->at(0).at(0).toString(), key);
    }

    QCOMPARE(retrievedSpy_->count(), 0);
    QCOMPARE(notFoundSpy_->count(), 0);
}

void CredentialServiceTest::removeSecret_rejectsBlankKeys()
{
    for(const QString& key : blankKeys)
    {
        errorSpy_->clear();
        service_->removeSecret(key);

        QCOMPARE(errorSpy_->count(), 1);
        QCOMPARE(errorSpy_->at(0).at(0).toString(), key);
    }

    QCOMPARE(removedSpy_->count(), 0);
    QCOMPARE(notFoundSpy_->count(), 0);
}

void CredentialServiceTest::setSecret_rejectsBlankSecrets()
{
    for(const QString& secret : blankSecrets)
    {
        errorSpy_->clear();
        service_->setSecret(QStringLiteral("steam_api_key"), secret);

        QCOMPARE(errorSpy_->count(), 1);
        QCOMPARE(errorSpy_->at(0).at(0).toString(), QStringLiteral("steam_api_key"));
        QVERIFY(errorSpy_->at(0).at(1).toString().contains(QStringLiteral("secret")));
    }

    QCOMPARE(storedSpy_->count(), 0);
}

void CredentialServiceTest::setSecret_doesNotRemoveExistingValueForBlankSecret()
{
    service_->setSecret(QStringLiteral("steam_api_key"), QStringLiteral("   "));

    QCOMPARE(errorSpy_->count(), 1);

    // Removal stays an explicit removeSecret() operation: no delete job -- and
    // no write job that would clear the stored value -- may be requested.
    QCOMPARE(service_->deleteJobRequests(), 0);
    QCOMPARE(service_->writeJobRequests(), 0);
    QCOMPARE(removedSpy_->count(), 0);
}

void CredentialServiceTest::blankRequests_neverReachTheKeychain()
{
    service_->setSecret(QStringLiteral("  "), QStringLiteral("secret"));
    service_->getSecret(QStringLiteral("  "));
    service_->removeSecret(QStringLiteral("  "));
    service_->setSecret(QStringLiteral("valid_key"), QStringLiteral("  "));

    QCOMPARE(errorSpy_->count(), 4);
    QCOMPARE(service_->totalJobRequests(), 0);
}

void CredentialServiceTest::setSecret_reportsErrorWhenWriteJobCannotBeCreated()
{
    service_->setSecret(QStringLiteral("steam_api_key"), QStringLiteral("secret"));

    QCOMPARE(service_->writeJobRequests(), 1);
    QCOMPARE(errorSpy_->count(), 1);
    QCOMPARE(errorSpy_->at(0).at(0).toString(), QStringLiteral("steam_api_key"));
    QVERIFY(errorSpy_->at(0).at(1).toString().contains(QStringLiteral("Unable to create")));
    QCOMPARE(storedSpy_->count(), 0);
}

void CredentialServiceTest::getSecret_reportsErrorWhenReadJobCannotBeCreated()
{
    service_->getSecret(QStringLiteral("player_id_key"));

    QCOMPARE(service_->readJobRequests(), 1);
    QCOMPARE(errorSpy_->count(), 1);
    QCOMPARE(errorSpy_->at(0).at(0).toString(), QStringLiteral("player_id_key"));
    QVERIFY(errorSpy_->at(0).at(1).toString().contains(QStringLiteral("Unable to create")));
    QCOMPARE(retrievedSpy_->count(), 0);
    QCOMPARE(notFoundSpy_->count(), 0);
}

void CredentialServiceTest::removeSecret_reportsErrorWhenDeleteJobCannotBeCreated()
{
    service_->removeSecret(QStringLiteral("steam_api_key"));

    QCOMPARE(service_->deleteJobRequests(), 1);
    QCOMPARE(errorSpy_->count(), 1);
    QVERIFY(errorSpy_->at(0).at(1).toString().contains(QStringLiteral("Unable to create")));
    QCOMPARE(removedSpy_->count(), 0);
}

void CredentialServiceTest::steamCredentialKeys_areDistinctAndNonBlank()
{
    const QString apiKey = QString::fromLatin1(CredentialService::kSteamApiKey);
    const QString playerIdKey = QString::fromLatin1(CredentialService::kSteamPlayerIdKey);

    QVERIFY(!apiKey.trimmed().isEmpty());
    QVERIFY(!playerIdKey.trimmed().isEmpty());
    QVERIFY(apiKey != playerIdKey);
}

QTEST_GUILESS_MAIN(CredentialServiceTest)

#include "CredentialServiceTest.moc"
