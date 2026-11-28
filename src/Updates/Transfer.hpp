#pragma once
#include <QObject>
#include <QPointer>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QTimer>
#include <functional>

namespace Acheron::Updates {
// Single asynchronous request, with a bounded body and an end-to-end deadline.
class Transfer : public QObject {
public:
    using Sink = std::function<bool(const QByteArray &)>;
    using Done = std::function<void(QString,int)>;
    explicit Transfer(QNetworkAccessManager &manager);
    ~Transfer() override;
    void get(QUrl url, qint64 limit, bool asset, Sink sink, Done done, int timeoutMs = 45000);
    void cancel();
    static bool allowedRedirect(const QUrl &url);
private:
    QNetworkAccessManager &network;
    QPointer<QNetworkReply> reply;
    QTimer deadline, idle;
    Sink consume;
    Done completed;
    qint64 remaining = 0;
    bool asset = false;
    int redirects = 0;
    void open(const QUrl &url);
    void drain();
    void finish(QString error, int status = 0);
};
}
