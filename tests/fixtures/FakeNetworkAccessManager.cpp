#include "fixtures/FakeNetworkAccessManager.h"

#include <algorithm>
#include <utility>

#include <QTimer>

namespace gamelog::tests::fixtures
{
    FakeNetworkReply::FakeNetworkReply(const QNetworkRequest& request,
                                       const QNetworkAccessManager::Operation operation,
                                       const int httpStatusCode,
                                       const NetworkError networkError,
                                       QByteArray body,
                                       QObject* parent)
        : QNetworkReply{parent},
          body_{std::move(body)}
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpStatusCode);

        if(networkError != NoError) { setError(networkError, QStringLiteral("Simulated network failure.")); }

        open(ReadOnly | Unbuffered);

        // Production code connects to finished() after the get() call returns, so
        // completion must be deferred to the event loop rather than emitted here.
        QTimer::singleShot(0,
                           this,
                           [this, networkError]
                           {
                               if(networkError != NoError) { emit errorOccurred(networkError); }

                               setFinished(true);
                               emit finished();
                           });
    }

    void FakeNetworkReply::abort() {}

    qint64 FakeNetworkReply::readData(char* data, const qint64 maxSize)
    {
        if(offset_ >= body_.size()) { return -1; }

        const qint64 available = body_.size() - offset_;
        const qint64 count = std::min(maxSize, available);

        std::copy_n(body_.constData() + offset_, count, data);
        offset_ += count;
        return count;
    }

    FakeNetworkAccessManager::FakeNetworkAccessManager(QObject* parent) : QNetworkAccessManager{parent} {}

    void FakeNetworkAccessManager::setResponseForUrlContaining(const QString& urlFragment, const FakeResponse& response)
    {
        for(auto& [fragment, existing] : responses_)
        {
            if(fragment == urlFragment)
            {
                existing = response;
                return;
            }
        }

        responses_.push_back({urlFragment, response});
    }

    void FakeNetworkAccessManager::setDefaultResponse(const FakeResponse& response) { defaultResponse_ = response; }

    const QList<QNetworkRequest>& FakeNetworkAccessManager::recordedRequests() const noexcept
    {
        return recordedRequests_;
    }

    int FakeNetworkAccessManager::requestCount() const noexcept { return static_cast<int>(recordedRequests_.size()); }

    void FakeNetworkAccessManager::clearRecordedRequests() { recordedRequests_.clear(); }

    QNetworkReply* FakeNetworkAccessManager::createRequest(const Operation operation,
                                                           const QNetworkRequest& request,
                                                           QIODevice* outgoingData)
    {
        Q_UNUSED(outgoingData)

        recordedRequests_.push_back(request);

        const FakeResponse response = responseFor(request.url());
        auto* reply = new FakeNetworkReply{
            request, operation, response.httpStatusCode, response.networkError, response.body, this
        };

        // QNetworkAccessManager::get() passes whatever createRequest() returns
        // through postProcess(), which is what wires the reply's finished()
        // signal to the manager-level finished(QNetworkReply*). Emitting it here
        // as well would deliver every reply to production code twice.
        return reply;
    }

    FakeResponse FakeNetworkAccessManager::responseFor(const QUrl& url) const
    {
        const QString urlString = url.toString();

        for(const auto& [fragment, response] : responses_) { if(urlString.contains(fragment)) { return response; } }

        return defaultResponse_;
    }
} // namespace gamelog::tests::fixtures
