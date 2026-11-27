#pragma once
#include "Registry.hpp"
#include <QFileSystemWatcher>
#include <QHash>
#include <QSet>
#include <QTimer>

namespace Acheron::Hub {

class SessionManager : public QObject
{
    Q_OBJECT
public:
    explicit SessionManager(Registry &registry);
    enum State { Closed, Launching, Open, Connecting, NeedsAuth, GatewayError, Locked, Failed };
    static State reportedState(const QByteArray &reply)
    {
        if (reply == "window") return Open;
        if (reply == "connecting") return Connecting;
        if (reply == "needs-auth") return NeedsAuth;
        if (reply == "gateway-error") return GatewayError;
        return Locked;
    }
    State state(const QString &id) const { return states.value(id, Closed); }
    bool controllable(const QString &id) const;
    void refresh();
    void open(const QString &id, bool add = false);
    void close(const QString &id, bool preserve);
    void enqueue(const QStringList &ids);
    bool busy(const QString &id) const;
    void cancelQueue() { queue.clear(); }

signals:
    void changed();
    void problem(const QString &message);

private:
    Registry &registry;
    QFileSystemWatcher watcher;
    QTimer monitor, next;
    QStringList known, queue;
    QHash<QString, State> states;
    QHash<QString, qint64> deadlines;
    QSet<QString> probing;
    void setState(const QString &id, State value);
    bool locked(const QString &id) const;
};
}
