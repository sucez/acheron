#pragma once
#include <QList>
#include <QByteArray>
namespace Acheron::Updates {
QList<QByteArray> productionKeys();
bool verifySignature(const QByteArray &manifest, const QByteArray &envelope, const QList<QByteArray> &pins);
}
