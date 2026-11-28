#include "PackageFile.hpp"
#include <QDir>
#include <QStorageInfo>
#include <QRegularExpression>
#include <QUuid>
namespace Acheron::Updates {
PackageFile::PackageFile(QString path, Package p) : root(std::move(path)), package(std::move(p)) {}
bool PackageFile::create()
{
    if (root.isEmpty() || !QDir().mkpath(root) || QFileInfo(root).isSymLink()) return false;
    QStorageInfo storage(root);
    if (!storage.isReady() || storage.bytesAvailable() < package.size + 1024*1024) {
        error = QObject::tr("Insufficient or unavailable disk space.");
        return false;
    }
    incoming = std::make_unique<QTemporaryDir>(QDir(root).filePath("incoming-XXXXXX"));
    if (!incoming->isValid()) return false;
    file.setFileName(incoming->filePath(package.filename));
    return file.open(QIODevice::WriteOnly | QIODevice::NewOnly);
}
bool PackageFile::reuse(const QString &directory)
{
    static const QRegularExpression valid("\\Aready-[a-f0-9]{32}\\z");
    if (!valid.match(directory).hasMatch() || QFileInfo(root).isSymLink()) return false;
    const auto dir = QDir(root).filePath(directory);
    if (QFileInfo(dir).isSymLink()) return false;
    auto path = QDir(dir).filePath(package.filename);
    QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() != package.size) return false;
    file.setFileName(path);
    existing = dir;
    written = info.size();
    return true;
}
bool PackageFile::append(const QByteArray &bytes)
{
    if (!incoming || bytes.size() > package.size - written || file.write(bytes) != bytes.size()) return false;
    written += bytes.size();
    return true;
}
bool PackageFile::beginVerification()
{
    if (written != package.size || (!reused() && !file.flush())) return false;
    file.close();
    return file.open(QIODevice::ReadOnly) && file.size() == package.size;
}
PackageFile::Result PackageFile::verifyBlock()
{
    const auto block = file.read(256*1024);
    if (file.error() != QFileDevice::NoError || block.size() > package.size - checked) return Invalid;
    hash.addData(block);
    checked += block.size();
    if (!file.atEnd()) return block.isEmpty() ? Invalid : More;
    file.close();
    verified = checked == package.size && hash.result() == package.digest;
    return verified ? Valid : Invalid;
}
QString PackageFile::publish(const QByteArray &manifest, const QByteArray &signature)
{
    if (!verified) return {};
    if (reused()) return file.fileName();
    const auto save = [&](const QString &name, const QByteArray &bytes) {
        QFile out(incoming->filePath(name));
        return out.open(QIODevice::WriteOnly | QIODevice::NewOnly) && out.write(bytes) == bytes.size() && out.flush();
    };
    if (!save("manifest.json",manifest) || !save("manifest.json.sig",signature)) return {};
    const auto name = "ready-" + QUuid::createUuid().toString(QUuid::Id128);
    const auto dest = QDir(root).filePath(name);
    if (!QDir().rename(incoming->path(),dest)) return {};
    incoming->setAutoRemove(false);
    return QDir(dest).filePath(package.filename);
}
}
