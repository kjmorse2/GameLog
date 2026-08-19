#pragma once

#include <QByteArray>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>

namespace gamelog::tests::fixtures
{
    /**
     * @brief A QNetworkReply that serves caller-supplied bytes without any network.
     *
     * QNetworkReply leaves abort() pure virtual and readData() to QIODevice, and
     * exposes the status/error setters as protected, so a small subclass is the
     * supported way to produce a canned reply.
     *
     * finished() is emitted asynchronously because production code connects to it
     * only after the get() call that creates the reply has already returned.
     */
    class FakeNetworkReply : public QNetworkReply
    {
        Q_OBJECT

    public:
        FakeNetworkReply(const QNetworkRequest& request,
                         QNetworkAccessManager::Operation operation,
                         int httpStatusCode,
                         NetworkError networkError,
                         QByteArray body,
                         QObject* parent = nullptr);

        void abort() override;

    protected:
        qint64 readData(char* data, qint64 maxSize) override;

    private:
        QByteArray body_;
        qint64 offset_{0};
    };

    /**
     * @brief One canned HTTP response.
     */
    struct FakeResponse
    {
        int httpStatusCode{200};
        QNetworkReply::NetworkError networkError{QNetworkReply::NoError};
        QByteArray body;
    };

    /**
     * @brief A QNetworkAccessManager that answers from a table of canned responses.
     *
     * createRequest() is virtual and protected on QNetworkAccessManager, which is
     * the seam used here. The manager still routes the returned reply through
     * postProcess(), so QNetworkAccessManager::finished(QNetworkReply*) -- the
     * signal GameArtworkService listens on -- is raised by the base class and
     * must not be emitted again here.
     */
    class FakeNetworkAccessManager : public QNetworkAccessManager
    {
        Q_OBJECT

    public:
        explicit FakeNetworkAccessManager(QObject* parent = nullptr);

        /**
         * Registers the response returned for any request whose URL contains
         * @p urlFragment. Later registrations replace earlier equal fragments.
         */
        void setResponseForUrlContaining(const QString& urlFragment, const FakeResponse& response);

        /**
         * Sets the response used when no registered fragment matches.
         */
        void setDefaultResponse(const FakeResponse& response);

        /**
         * @return Every request this manager was asked to create, in order.
         */
        [[nodiscard]] const QList<QNetworkRequest>& recordedRequests() const noexcept;

        /**
         * @return The number of requests this manager was asked to create.
         */
        [[nodiscard]] int requestCount() const noexcept;

        /**
         * Forgets all recorded requests without clearing registered responses.
         */
        void clearRecordedRequests();

    protected:
        QNetworkReply* createRequest(Operation operation,
                                     const QNetworkRequest& request,
                                     QIODevice* outgoingData) override;

    private:
        [[nodiscard]] FakeResponse responseFor(const QUrl& url) const;

        QList<QPair<QString, FakeResponse>> responses_;
        FakeResponse defaultResponse_;
        QList<QNetworkRequest> recordedRequests_;
    };
} // namespace gamelog::tests::fixtures
