#pragma once
#include "Manifest.hpp"
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <memory>

namespace Acheron::Updates {
struct VerifiedPackage {
    QString path;
    Package distribution;
    QByteArray manifest, signature;
};
class PackageFile {
public:
    enum Result { More, Valid, Invalid };
    PackageFile(QString root, Package package);
    bool create();
    bool reuse(const QString &directory);
    bool append(const QByteArray &bytes);
    bool beginVerification();
    Result verifyBlock();
    QString publish(const QByteArray &manifest, const QByteArray &signature);
    qint64 received() const { return written; }
    bool reused() const { return !existing.isEmpty(); }
    QString error;
private:
    QString root, existing;
    Package package;
    std::unique_ptr<QTemporaryDir> incoming;
    QFile file;
    QCryptographicHash hash{QCryptographicHash::Sha256};
    qint64 written = 0, checked = 0;
    bool verified = false;
};
}
