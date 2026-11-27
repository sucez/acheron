#include "LocalControl.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QLocalSocket>
#include <QTimer>
#include <QStandardPaths>
#include <memory>

namespace Acheron::Hub {

QString endpoint(const QString &directory)
{
    const auto identity = QDir::cleanPath(directory) + QLatin1Char('|') + QDir::homePath();
    const auto name = QStringLiteral("acheron-v1-") + QString::fromLatin1(
            QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
#ifdef Q_OS_LINUX
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!runtime.isEmpty())
        return QDir(runtime).filePath(name);
#endif
    return name;
}

bool listen(QLocalServer &server, const QString &directory, Handler handle)
{
    // Caller holds the associated QLockFile throughout the server's lifetime.
    const auto name = endpoint(directory);
    server.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(name);
    QObject::connect(&server, &QLocalServer::newConnection, &server, [&server, handle] {
        while (auto *peer = server.nextPendingConnection()) {
            peer->setReadBufferSize(65);
            QObject::connect(peer, &QLocalSocket::disconnected, peer, &QObject::deleteLater);
            QTimer::singleShot(1500, peer, [peer] { peer->abort(); peer->deleteLater(); });
            auto read = [peer, handle] {
                if (peer->bytesAvailable() > 64) {
                    peer->abort();
                    return;
                }
                if (!peer->canReadLine())
                    return;
                auto line = peer->readAll();
                auto reply = QByteArray("error");
                if (line.startsWith("1 ") && line.endsWith('\n') && line.count('\n') == 1)
                    reply = handle(line.mid(2, line.size() - 3));
                peer->write("1 " + reply + "\n");
                peer->disconnectFromServer();
            };
            QObject::connect(peer, &QLocalSocket::readyRead, peer, read);
            read();
        }
    });
    return server.listen(name);
}

void request(QObject *owner, const QString &directory, const QByteArray &verb,
             std::function<void(QByteArray)> finished)
{
    auto *peer = new QLocalSocket(owner);
    peer->setReadBufferSize(65);
    auto pending = std::make_shared<bool>(true);
    auto done = [peer, pending, finished](QByteArray reply) {
        if (!*pending)
            return;
        *pending = false;
        peer->deleteLater();
        finished(reply);
    };
    QObject::connect(peer, &QLocalSocket::connected, peer, [peer, verb] { peer->write("1 " + verb + "\n"); });
    QObject::connect(peer, &QLocalSocket::readyRead, peer, [peer, done] {
        if (!peer->canReadLine())
            return;
        const auto line = peer->readAll();
        done(line.size() <= 64 && line.startsWith("1 ") && line.endsWith('\n') && line.count('\n') == 1
             ? line.mid(2, line.size() - 3) : QByteArray());
    });
    QObject::connect(peer, &QLocalSocket::disconnected, peer, [done] { done({}); });
    QTimer::singleShot(1200, peer, [done] { done({}); });
    peer->connectToServer(endpoint(directory));
}

} // namespace Acheron::Hub
