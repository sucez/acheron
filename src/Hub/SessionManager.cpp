#include "SessionManager.hpp"
#include "CardMetadata.hpp"
#include "LocalControl.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>

namespace Acheron::Hub {

SessionManager::SessionManager(Registry &r) : registry(r)
{
    watcher.addPath(r.root());
    monitor.setInterval(2000);
    connect(&monitor, &QTimer::timeout, this, &SessionManager::refresh);
    next.setInterval(450);
    connect(&next, &QTimer::timeout, this, [this] {
        if (queue.isEmpty()) {
            next.stop();
            return;
        }
        const QString id = queue.takeFirst();
        if (!busy(id) && !registry.option("archived/" + id).toBool())
            open(id);
    });
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &dir) {
        refresh();
        const auto preview = CardMetadata::path(dir);
        if (QFileInfo::exists(preview) && !watcher.files().contains(preview)) {
            watcher.addPath(preview);
            emit changed();
        }
    });
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        if (QFileInfo::exists(path) && !watcher.files().contains(path))
            watcher.addPath(path);
        emit changed();
    });
    refresh();
}

void SessionManager::setState(const QString &id, State value)
{
    if (state(id) != value) {
        states[id] = value;
        emit changed();
    }
}

bool SessionManager::locked(const QString &id) const
{
    const QString file = registry.directory(id) + "/profile.lock";
    if (!QFileInfo::exists(file))
        return false;
    QLockFile check(file);
    check.setStaleLockTime(0);
    return !check.tryLock(0);
}

bool SessionManager::busy(const QString &id) const
{
    return deadlines.contains(id) || locked(id);
}

bool SessionManager::controllable(const QString &id) const
{
    const auto value = state(id);
    return value == Open || value == Connecting || value == NeedsAuth || value == GatewayError;
}

void SessionManager::refresh()
{
    const auto ids = registry.profiles();
    if (ids != known) {
        known = ids;
        for (const QString &id : ids) {
            const auto dir = registry.directory(id);
            if (!watcher.directories().contains(dir))
                watcher.addPath(dir);
            const auto preview = CardMetadata::path(dir);
            if (QFileInfo::exists(preview) && !watcher.files().contains(preview))
                watcher.addPath(preview);
        }
        emit changed();
    }
    bool active = false;
    for (const QString &id : ids) {
        if (!locked(id)) {
            if (deadlines.contains(id)) {
                if (QDateTime::currentMSecsSinceEpoch() < deadlines.value(id)) {
                    active = true;
                    continue;
                }
                deadlines.remove(id);
                setState(id, Failed);
            } else if (state(id) != Failed) {
                setState(id, Closed);
            }
            continue;
        }
        active = true;
        if (state(id) == Closed || state(id) == Failed)
            setState(id, Locked);
        if (probing.contains(id))
            continue;
        probing.insert(id);
        request(this, registry.directory(id), "status", [this, id](QByteArray reply) {
            probing.remove(id);
            if (!locked(id))
                return;
            const State state = reportedState(reply);
            if (state != Locked) {
                deadlines.remove(id);
                setState(id, state);
            } else if (deadlines.value(id) <= QDateTime::currentMSecsSinceEpoch()) {
                deadlines.remove(id);
                setState(id, Locked);
            }
        });
    }
    if (active && !monitor.isActive())
        monitor.start();
    else if (!active)
        monitor.stop();
}

void SessionManager::open(const QString &id, bool add)
{
    if (!registry.profiles().contains(id) || deadlines.contains(id))
        return;
    if (locked(id)) {
        registry.set("used/" + id, QDateTime::currentMSecsSinceEpoch());
        emit changed();
        request(this, registry.directory(id), state(id) == NeedsAuth ? "login" : "activate", [this](QByteArray reply) {
            if (reply != "ok")
                emit problem(tr("The profile is in use. Switch to its window from the taskbar."));
        });
        return;
    }
    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                 {"--profile", id, add ? "--hub-add" : "--hub-login"})) {
        setState(id, Failed);
        return;
    }
    registry.set("used/" + id, QDateTime::currentMSecsSinceEpoch());
    deadlines[id] = QDateTime::currentMSecsSinceEpoch() + 15000;
    setState(id, Launching);
    monitor.start();
}

void SessionManager::close(const QString &id, bool preserve)
{
    request(this, registry.directory(id), preserve ? "suspend" : "close", [this](QByteArray reply) {
        if (reply != "ok")
            emit problem(tr("The session did not acknowledge the close request."));
    });
}

void SessionManager::enqueue(const QStringList &ids)
{
    for (const auto &id : ids)
        if (!queue.contains(id))
            queue.append(id);
    if (!queue.isEmpty())
        next.start();
}

} // namespace Acheron::Hub
