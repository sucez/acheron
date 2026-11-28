#include "Transfer.hpp"
#include <QNetworkRequest>
namespace Acheron::Updates {
Transfer::Transfer(QNetworkAccessManager &manager) : network(manager)
{
    deadline.setSingleShot(true);
    idle.setSingleShot(true);
    for (auto *timer : {&deadline, &idle})
        connect(timer, &QTimer::timeout, this, [this] { finish(tr("Request timed out.")); });
}
Transfer::~Transfer() { cancel(); }

bool Transfer::allowedRedirect(const QUrl &url)
{
    return url.isValid() && url.scheme() == "https" && url.userInfo().isEmpty()
        && url.port(443) == 443 && !url.hasFragment()
        && (url.host() == "release-assets.githubusercontent.com" || url.host() == "objects.githubusercontent.com");
}

void Transfer::cancel()
{
    deadline.stop();
    idle.stop();
    if (reply) {
        auto *old = reply.data();
        reply.clear();
        old->disconnect(this);
        if (!old->isFinished()) old->abort();
        old->deleteLater();
    }
    consume = {};
    completed = {};
}

void Transfer::finish(QString error, int status)
{
    auto callback = std::move(completed);
    cancel();
    if (callback) callback(error,status);
}

void Transfer::get(QUrl url, qint64 limit, bool isAsset, Sink sink, Done done, int timeoutMs)
{
    cancel();
    remaining = limit;
    asset = isAsset;
    redirects = 0;
    consume = std::move(sink);
    completed = std::move(done);
    deadline.start(timeoutMs);
    open(url);
}

void Transfer::open(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent","Acheron-Custom-Update-Center");
    request.setRawHeader("Accept-Encoding","identity");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    for (auto attr : {QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::CookieSaveControlAttribute,
                      QNetworkRequest::AuthenticationReuseAttribute})
        request.setAttribute(attr,QNetworkRequest::Manual);
    reply = network.get(request);
    reply->setReadBufferSize(64*1024);
    idle.start(20000);
    connect(reply,&QIODevice::readyRead,this,&Transfer::drain);
    connect(reply,&QNetworkReply::finished,this,[this] {
        auto *current = reply.data();
        drain();
        if (!current || current != reply.data()) return;
        int status = current->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            auto target = current->url().resolved(current->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            if (!asset || ++redirects > 3 || !allowedRedirect(target)) {
                finish(tr("Redirect refused."),status);
                return;
            }
            current->disconnect(this);
            current->deleteLater();
            open(target);
            return;
        }
        if (current->error() != QNetworkReply::NoError || status != 200) {
            auto message = status == 403 || status == 429 ? tr("GitHub refused the request or its rate limit was reached.")
                        : status ? tr("HTTP error %1.").arg(status) : tr("Network or TLS connection failed.");
            finish(message,status);
            return;
        }
        finish({},status);
    });
}

void Transfer::drain()
{
    if (!reply || !reply->isOpen()) return;
    while (reply && reply->bytesAvailable()) {
        const auto bytes = reply->read(64*1024);
        if (bytes.isEmpty()) break;
        idle.start(20000);
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) continue;
        if (bytes.size() > remaining) {
            finish(tr("Response exceeds the permitted size."),200);
            return;
        }
        remaining -= bytes.size();
        if (!consume(bytes)) {
            finish(tr("Cannot store the response."),200);
            return;
        }
    }
}
}
