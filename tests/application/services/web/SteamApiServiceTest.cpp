#include <QtTest/QtTest>

#include "application/services/local/CredentialService.h"
#include "application/services/web/SteamApiService.h"
#include "fixtures/FakeNetworkAccessManager.h"

#include <algorithm>
#include <chrono>
#include <memory>

#include <QJsonArray>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QStringList>
#include <QUrlQuery>

using gamelog::application::services::CredentialService;
using gamelog::application::services::SteamApiService;
using gamelog::tests::fixtures::FakeNetworkAccessManager;
using gamelog::tests::fixtures::FakeResponse;

namespace
{
    /**
     * Refuses to create keychain jobs so no test can reach the real keychain.
     *
     * The Steam state machine is driven by emitting CredentialService's signals
     * directly, which is possible because Q_SIGNALS expands to public.
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

        void resetReadJobRequests() noexcept { readJobRequests_ = 0; }

    private:
        int readJobRequests_{0};
    };

    QByteArray ownedGamesBody(const int gameCount)
    {
        QString games;

        for(int index = 0; index < gameCount; ++index)
        {
            if(index > 0) { games += QLatin1Char(','); }
            games += QStringLiteral(R"({"appid":%1,"name":"Game %1"})").arg(index + 1);
        }

        return QStringLiteral(R"({"response":{"game_count":%1,"games":[%2]}})").arg(gameCount).arg(games).toUtf8();
    }

    QStringList* capturedMessages = nullptr;
    QtMessageHandler previousMessageHandler = nullptr;

    /**
     * Records every log line so the service's own logging can be asserted on.
     */
    void capturingMessageHandler(const QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        if(capturedMessages != nullptr) { capturedMessages->push_back(message); }
        if(previousMessageHandler != nullptr) { previousMessageHandler(type, context, message); }
    }

    QLoggingCategory::CategoryFilter previousCategoryFilter = nullptr;

    /**
     * Force-enables the Steam API category no matter how logging is configured.
     *
     * Rules from QT_LOGGING_RULES and qtlogging.ini are applied after, and so
     * override, QLoggingCategory::setFilterRules(). An installed filter is the
     * only mechanism that outranks both, which keeps this test's assertions
     * meaningful instead of silently capturing nothing.
     */
    void enableSteamApiCategory(QLoggingCategory* category)
    {
        if(previousCategoryFilter != nullptr) { previousCategoryFilter(category); }

        if(qstrcmp(category->categoryName(), "gamelog.application.services.steam_api") == 0)
        {
            category->setEnabled(QtDebugMsg, true);
            category->setEnabled(QtInfoMsg, true);
            category->setEnabled(QtWarningMsg, true);
        }
    }

    constexpr auto kValidPlayerId = "76561197960287930";
    constexpr auto kValidApiKey = "0123456789ABCDEF0123456789ABCDEF";
} // namespace

namespace
{
    class SteamApiServiceTest : public QObject
    {
        Q_OBJECT

    private
        slots  :
        void init();

        void cleanup();

        void getOwnedGames_requestsBothCredentials();

        void getOwnedGames_issuesOneRequestAndEmitsOwnedGames();

        void getOwnedGames_acceptsCredentialsInEitherOrder();

        void getOwnedGames_failsWhileAnotherRequestIsInFlight();

        void getOwnedGames_ignoresDuplicateSecretRetrieved();

        void getOwnedGames_ignoresUnrelatedCredentialKeys();

        void getOwnedGames_ignoresCredentialsWithoutAnActiveRequest();

        void getOwnedGames_failsForMissingSecret();

        void getOwnedGames_failsForMalformedJson();

        void getOwnedGames_failsForHttpError();

        void getOwnedGames_failsForNonNumericPlayerId();

        void getOwnedGames_failsForZeroPlayerId();

        void getOwnedGames_failsImmediatelyForBlankApiKey();

        void getOwnedGames_failsImmediatelyForBlankPlayerId();

        void getOwnedGames_clearsStateAfterCredentialFailure();

        void getOwnedGames_sendsApiKeyOnlyAsQueryParameter();

        void getOwnedGames_neverLogsTheApiKeyOrTheQueryString();

        void getOwnedGames_acceptsEmptyGamesArrayAsSuccess();

        void getOwnedGames_failsForResponseShapesWithoutAGamesArray();

        void getOwnedGames_recoversWhenCredentialCallbacksNeverArrive();

    private:
        void startRequestSilently() const;

        void deliverCredentials(const QString& apiKey = QString::fromLatin1(kValidApiKey),
                                const QString& playerId = QString::fromLatin1(kValidPlayerId)) const;

        [[nodiscard]] static QString apiKeyName();

        [[nodiscard]] static QString playerIdKeyName();

        std::unique_ptr<KeychainFreeCredentialService> credentialService_;
        std::unique_ptr<FakeNetworkAccessManager> networkAccessManager_;
        std::unique_ptr<SteamApiService> service_;
        std::unique_ptr<QSignalSpy> receivedSpy_;
        std::unique_ptr<QSignalSpy> failedSpy_;
    };
} // namespace

QString SteamApiServiceTest::apiKeyName() { return QString::fromLatin1(CredentialService::kSteamApiKey); }

QString SteamApiServiceTest::playerIdKeyName() { return QString::fromLatin1(CredentialService::kSteamPlayerIdKey); }

void SteamApiServiceTest::init()
{
    credentialService_ = std::make_unique<KeychainFreeCredentialService>();
    networkAccessManager_ = std::make_unique<FakeNetworkAccessManager>();
    service_ = std::make_unique<SteamApiService>(*credentialService_, *networkAccessManager_);

    receivedSpy_ = std::make_unique<QSignalSpy>(service_.get(), &SteamApiService::ownedGamesReceived);
    failedSpy_ = std::make_unique<QSignalSpy>(service_.get(), &SteamApiService::requestFailed);

    QVERIFY(receivedSpy_->isValid());
    QVERIFY(failedSpy_->isValid());

    networkAccessManager_->setDefaultResponse(FakeResponse{200, QNetworkReply::NoError, ownedGamesBody(3)});
}

void SteamApiServiceTest::cleanup()
{
    failedSpy_.reset();
    receivedSpy_.reset();
    service_.reset();
    networkAccessManager_.reset();
    credentialService_.reset();
}

void SteamApiServiceTest::startRequestSilently() const
{
    // getOwnedGames() asks CredentialService for both secrets. Job creation is
    // already stubbed out, so blocking the resulting error signals keeps the
    // request pending until the test delivers credentials itself.
    credentialService_->blockSignals(true);
    service_->getOwnedGames();
    credentialService_->blockSignals(false);
}

void SteamApiServiceTest::deliverCredentials(const QString& apiKey, const QString& playerId) const
{
    credentialService_->secretRetrieved(apiKeyName(), apiKey);
    credentialService_->secretRetrieved(playerIdKeyName(), playerId);
}

void SteamApiServiceTest::getOwnedGames_requestsBothCredentials()
{
    QStringList requestedKeys;
    connect(credentialService_.get(),
            &CredentialService::credentialError,
            this,
            [&requestedKeys](const QString& key, const QString&) { requestedKeys.push_back(key); });

    service_->getOwnedGames();

    QCOMPARE(credentialService_->readJobRequests(), 2);
    QVERIFY(requestedKeys.contains(apiKeyName()));
    QVERIFY(requestedKeys.contains(playerIdKeyName()));

    // A credential error also fails the in-flight request rather than hanging.
    QCOMPARE(failedSpy_->count(), 1);
    QCOMPARE(networkAccessManager_->requestCount(), 0);
}

void SteamApiServiceTest::getOwnedGames_issuesOneRequestAndEmitsOwnedGames()
{
    startRequestSilently();
    QCOMPARE(networkAccessManager_->requestCount(), 0);

    deliverCredentials();
    QCOMPARE(networkAccessManager_->requestCount(), 1);

    QVERIFY(receivedSpy_->wait(2000));
    QCOMPARE(receivedSpy_->count(), 1);
    QCOMPARE(failedSpy_->count(), 0);

    const QJsonArray games = receivedSpy_->at(0).at(0).toJsonArray();
    QCOMPARE(static_cast<int>(games.size()), 3);
    QCOMPARE(games.at(0).toObject().value(QStringLiteral("appid")).toInt(), 1);
}

void SteamApiServiceTest::getOwnedGames_acceptsCredentialsInEitherOrder()
{
    startRequestSilently();

    credentialService_->secretRetrieved(playerIdKeyName(), QString::fromLatin1(kValidPlayerId));
    QCOMPARE(networkAccessManager_->requestCount(), 0);

    credentialService_->secretRetrieved(apiKeyName(), QString::fromLatin1(kValidApiKey));
    QCOMPARE(networkAccessManager_->requestCount(), 1);

    QVERIFY(receivedSpy_->wait(2000));
    QCOMPARE(receivedSpy_->count(), 1);
}

void SteamApiServiceTest::getOwnedGames_failsWhileAnotherRequestIsInFlight()
{
    startRequestSilently();

    service_->getOwnedGames();

    QCOMPARE(failedSpy_->count(), 1);
    QVERIFY(failedSpy_->at(0).at(0).toString().contains(QStringLiteral("already in progress")));

    // The second call must not have re-requested credentials either.
    QCOMPARE(credentialService_->readJobRequests(), 2);
}

void SteamApiServiceTest::getOwnedGames_ignoresDuplicateSecretRetrieved()
{
    startRequestSilently();
    deliverCredentials();
    QCOMPARE(networkAccessManager_->requestCount(), 1);

    // A duplicated completion must not open a second HTTP stream.
    credentialService_->secretRetrieved(apiKeyName(), QString::fromLatin1(kValidApiKey));
    credentialService_->secretRetrieved(playerIdKeyName(), QString::fromLatin1(kValidPlayerId));

    QCOMPARE(networkAccessManager_->requestCount(), 1);

    QVERIFY(receivedSpy_->wait(2000));
    QCOMPARE(receivedSpy_->count(), 1);
}

void SteamApiServiceTest::getOwnedGames_ignoresUnrelatedCredentialKeys()
{
    startRequestSilently();

    credentialService_->secretRetrieved(QStringLiteral("some_other_consumer"), QStringLiteral("value"));
    credentialService_->secretNotFound(QStringLiteral("some_other_consumer"));
    credentialService_->credentialError(QStringLiteral("some_other_consumer"), QStringLiteral("boom"));

    QCOMPARE(failedSpy_->count(), 0);
    QCOMPARE(networkAccessManager_->requestCount(), 0);

    deliverCredentials();
    QCOMPARE(networkAccessManager_->requestCount(), 1);
}

void SteamApiServiceTest::getOwnedGames_ignoresCredentialsWithoutAnActiveRequest()
{
    deliverCredentials();
    credentialService_->secretNotFound(apiKeyName());
    credentialService_->credentialError(playerIdKeyName(), QStringLiteral("boom"));

    QCOMPARE(networkAccessManager_->requestCount(), 0);
    QCOMPARE(failedSpy_->count(), 0);
    QCOMPARE(receivedSpy_->count(), 0);
}

void SteamApiServiceTest::getOwnedGames_failsForMissingSecret()
{
    startRequestSilently();
    credentialService_->secretNotFound(apiKeyName());

    QCOMPARE(failedSpy_->count(), 1);
    QVERIFY(failedSpy_->at(0).at(0).toString().contains(QStringLiteral("Steam API key")));
    QCOMPARE(networkAccessManager_->requestCount(), 0);

    startRequestSilently();
    credentialService_->secretNotFound(playerIdKeyName());

    QCOMPARE(failedSpy_->count(), 2);
    QVERIFY(failedSpy_->at(1).at(0).toString().contains(QStringLiteral("player ID")));
}

void SteamApiServiceTest::getOwnedGames_failsForMalformedJson()
{
    networkAccessManager_->setDefaultResponse(FakeResponse{
                                                  200, QNetworkReply::NoError, QByteArrayLiteral("{\"response\": ")
                                              });

    startRequestSilently();
    deliverCredentials();

    QVERIFY(failedSpy_->wait(2000));
    QCOMPARE(failedSpy_->count(), 1);
    QVERIFY(failedSpy_->at(0).at(0).toString().contains(QStringLiteral("parse")));
    QCOMPARE(receivedSpy_->count(), 0);
}

void SteamApiServiceTest::getOwnedGames_failsForHttpError()
{
    networkAccessManager_->setDefaultResponse(FakeResponse{
                                                  403, QNetworkReply::ContentAccessDenied,
                                                  QByteArrayLiteral("Forbidden")
                                              });

    startRequestSilently();
    deliverCredentials();

    QVERIFY(failedSpy_->wait(2000));
    QCOMPARE(failedSpy_->count(), 1);
    QVERIFY(failedSpy_->at(0).at(0).toString().contains(QStringLiteral("403")));
    QCOMPARE(receivedSpy_->count(), 0);
}

void SteamApiServiceTest::getOwnedGames_failsForNonNumericPlayerId()
{
    startRequestSilently();
    deliverCredentials(QString::fromLatin1(kValidApiKey), QStringLiteral("not-a-steam-id"));

    QCOMPARE(failedSpy_->count(), 1);
    QVERIFY(failedSpy_->at(0).at(0).toString().contains(QStringLiteral("invalid")));
    QCOMPARE(networkAccessManager_->requestCount(), 0);
}

void SteamApiServiceTest::getOwnedGames_failsForZeroPlayerId()
{
    startRequestSilently();
    deliverCredentials(QString::fromLatin1(kValidApiKey), QStringLiteral("0"));

    QCOMPARE(failedSpy_->count(), 1);
    QVERIFY(failedSpy_->at(0).at(0).toString().contains(QStringLiteral("invalid")));
    QCOMPARE(networkAccessManager_->requestCount(), 0);
}

void SteamApiServiceTest::getOwnedGames_failsImmediatelyForBlankApiKey()
{
    for(const QString& blankKey : {QStringLiteral(""), QStringLiteral("   "), QStringLiteral("\t\n")})
    {
        failedSpy_->clear();
        startRequestSilently();

        credentialService_->secretRetrieved(apiKeyName(), blankKey);

        QCOMPARE(failedSpy_->count(), 1);
        QVERIFY(failedSpy_->at(0).at(0).toString().contains(QStringLiteral("API key")));
        QCOMPARE(networkAccessManager_->requestCount(), 0);
    }
}

void SteamApiServiceTest::getOwnedGames_failsImmediatelyForBlankPlayerId()
{
    for(const QString& blankId : {QStringLiteral(""), QStringLiteral("   "), QStringLiteral("\t\n")})
    {
        failedSpy_->clear();
        startRequestSilently();

        credentialService_->secretRetrieved(playerIdKeyName(), blankId);

        QCOMPARE(failedSpy_->count(), 1);
        QVERIFY(failedSpy_->at(0).at(0).toString().contains(QStringLiteral("player ID")));
        QCOMPARE(networkAccessManager_->requestCount(), 0);
    }
}

void SteamApiServiceTest::getOwnedGames_clearsStateAfterCredentialFailure()
{
    startRequestSilently();
    credentialService_->secretRetrieved(apiKeyName(), QStringLiteral("   "));
    QCOMPARE(failedSpy_->count(), 1);

    // Request state must be cleared, so the next attempt is accepted instead of
    // being rejected as already in progress.
    failedSpy_->clear();
    startRequestSilently();
    deliverCredentials();

    QVERIFY(receivedSpy_->wait(2000));
    QCOMPARE(receivedSpy_->count(), 1);
    QCOMPARE(failedSpy_->count(), 0);
}

void SteamApiServiceTest::getOwnedGames_sendsApiKeyOnlyAsQueryParameter()
{
    startRequestSilently();
    deliverCredentials();

    QCOMPARE(networkAccessManager_->requestCount(), 1);

    const QNetworkRequest& request = networkAccessManager_->recordedRequests().at(0);
    const QUrlQuery query{request.url()};

    QCOMPARE(query.queryItemValue(QStringLiteral("key")), QString::fromLatin1(kValidApiKey));
    QCOMPARE(query.queryItemValue(QStringLiteral("steamid")), QString::fromLatin1(kValidPlayerId));

    QVERIFY(!request.hasRawHeader(QByteArrayLiteral("x-webapi-key")));
    QVERIFY(!request.hasRawHeader(QByteArrayLiteral("X-Webapi-Key")));

    // The key must not have leaked into any other header either.
    for(const QByteArray& headerName : request.rawHeaderList())
    {
        QVERIFY(!request.rawHeader(headerName).contains(QByteArray{kValidApiKey}));
    }

    QVERIFY(receivedSpy_->wait(2000));
}

void SteamApiServiceTest::getOwnedGames_neverLogsTheApiKeyOrTheQueryString()
{
    previousCategoryFilter = QLoggingCategory::installFilter(enableSteamApiCategory);

    QStringList messages;
    capturedMessages = &messages;
    previousMessageHandler = qInstallMessageHandler(capturingMessageHandler);

    startRequestSilently();
    deliverCredentials();
    const bool received = receivedSpy_->wait(2000);

    qInstallMessageHandler(previousMessageHandler);
    capturedMessages = nullptr;

    // Restoring the previous filter re-evaluates every category, returning
    // them to whatever the ambient configuration specified.
    QLoggingCategory::installFilter(previousCategoryFilter);
    previousCategoryFilter = nullptr;

    QVERIFY(received);

    // With the category force-enabled above this is an invariant of the
    // service, not of the environment: the request path always logs.
    QVERIFY(!messages.isEmpty());

    const QString apiKey = QString::fromLatin1(kValidApiKey);
    bool sawEndpointLine = false;

    for(const QString& message : messages)
    {
        QVERIFY2(!message.contains(apiKey), qPrintable(message));

        if(!message.contains(QStringLiteral("Endpoint:"))) { continue; }

        sawEndpointLine = true;

        // The endpoint is logged without its query string.
        QVERIFY2(!message.contains(QLatin1Char('?')), qPrintable(message));
        QVERIFY2(!message.contains(QStringLiteral("steamid=")), qPrintable(message));
        QVERIFY2(message.contains(QStringLiteral("GetOwnedGames")), qPrintable(message));
    }

    QVERIFY(sawEndpointLine);
}

void SteamApiServiceTest::getOwnedGames_recoversWhenCredentialCallbacksNeverArrive()
{
    // A keychain that neither succeeds nor errors used to leave requestInProgress_
    // set forever, rejecting every later request for the process lifetime.
    service_->setCredentialTimeout(std::chrono::milliseconds{50});

    startRequestSilently();

    // No credentials are ever delivered; only the guard can end this request.
    QVERIFY(failedSpy_->wait(2000));
    QCOMPARE(failedSpy_->count(), 1);
    QVERIFY(failedSpy_->at(0).at(0).toString().contains(QStringLiteral("Timed out")));
    QCOMPARE(networkAccessManager_->requestCount(), 0);

    // The service must accept work again rather than stay wedged.
    failedSpy_->clear();
    startRequestSilently();
    deliverCredentials();

    QVERIFY(receivedSpy_->wait(2000));
    QCOMPARE(failedSpy_->count(), 0);
}

void SteamApiServiceTest::getOwnedGames_acceptsEmptyGamesArrayAsSuccess()
{
    networkAccessManager_->setDefaultResponse(FakeResponse{
                                                  200, QNetworkReply::NoError,
                                                  QByteArrayLiteral(R"({"response":{"games":[]}})")
                                              });

    startRequestSilently();
    deliverCredentials();

    QVERIFY(receivedSpy_->wait(2000));
    QCOMPARE(receivedSpy_->count(), 1);
    QCOMPARE(failedSpy_->count(), 0);
    QVERIFY(receivedSpy_->at(0).at(0).toJsonArray().isEmpty());
}

void SteamApiServiceTest::getOwnedGames_failsForResponseShapesWithoutAGamesArray()
{
    const QList<QByteArray> invalidBodies{
        QByteArrayLiteral(R"({"response":{}})"), QByteArrayLiteral(R"([1,2,3])"),
        QByteArrayLiteral(R"({"response":"nope"})"), QByteArrayLiteral(R"({"response":{"games":"nope"}})"),
        QByteArrayLiteral(R"({"response":{"games":{}}})"), QByteArrayLiteral(R"({})")
    };

    for(const QByteArray& body : invalidBodies)
    {
        failedSpy_->clear();
        receivedSpy_->clear();
        networkAccessManager_->setDefaultResponse(FakeResponse{200, QNetworkReply::NoError, body});

        startRequestSilently();
        deliverCredentials();

        QVERIFY2(failedSpy_->wait(2000), body.constData());
        QCOMPARE(failedSpy_->count(), 1);
        QCOMPARE(receivedSpy_->count(), 0);
    }
}

QTEST_GUILESS_MAIN(SteamApiServiceTest)

#include "SteamApiServiceTest.moc"
