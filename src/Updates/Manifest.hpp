#pragma once
#include <QByteArray>
#include <QString>
#include <optional>

namespace Acheron::Updates {
inline constexpr int ManifestLimit = 32 * 1024;
inline constexpr qint64 PackageLimit = 2LL * 1024 * 1024 * 1024;
struct Package {
    QString version, channel, commit, filename, url, notes;
    QByteArray digest;
    qint64 size;
};
bool newer(const QString &candidate, const QString &installed, const QString &channel);
QString releasePage(const QString &version);
std::optional<Package> parseManifest(const QByteArray &bytes, const QString &candidate,
    const QString &installed, const QString &channel, const QString &os, const QString &arch);
}
