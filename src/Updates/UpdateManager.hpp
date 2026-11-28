#pragma once
#include "Manifest.hpp"
#include "Transfer.hpp"
#include "Hub/Registry.hpp"
namespace Acheron::Updates {
struct ClientIdentity { QString installed, buildChannel, os, arch; QList<QByteArray> keys; };
class UpdateManager : public QObject {
    Q_OBJECT
public:
    enum State { Idle, Checking, Current, Available, Unavailable, VerificationFailed, NetworkError,
                 Downloading, Verifying, Complete, Cancelled, StorageError };
    UpdateManager(Hub::Registry &registry, QNetworkAccessManager &network, ClientIdentity identity);
    State state() const { return phase; }
    QString message() const { return detail; }
    QString channel() const;
    bool automatic() const;
    qint64 lastCheck() const;
    bool preferences(QString channel, bool automatic);
    const ClientIdentity &client() const { return identity; }
    const std::optional<Package> &offer() const { return package; }
    void check();
    void startup();
signals:
    void changed();
private:
    Hub::Registry &registry;
    ClientIdentity identity;
    Transfer transfer;
    State phase = Idle;
    QString detail, best;
    bool sawCompatible = false;
    QByteArray manifest, signature;
    std::optional<Package> package;
    void set(State state, QString message = {});
    void success(State state);
    void list(int page);
    void authenticate();
    void fetchSmall(const QUrl &url, int limit, bool asset, std::function<void(QByteArray)> next);
};
}
