#pragma once

#include <QLocalServer>
#include <functional>

namespace Acheron::Hub {

// Endpoint identity includes the user's resolved application data directory.
QString endpoint(const QString &directory);
using Handler = std::function<QByteArray(const QByteArray &)>;
bool listen(QLocalServer &server, const QString &directory, Handler handle);
void request(QObject *owner, const QString &directory, const QByteArray &verb,
             std::function<void(QByteArray)> finished);

} // namespace Acheron::Hub
