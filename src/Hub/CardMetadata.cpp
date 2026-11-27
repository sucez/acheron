#include "CardMetadata.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace Acheron::Hub {

QString CardMetadata::path(const QString &directory) { return directory + "/hub-card.json"; }

std::optional<CardMetadata> CardMetadata::read(const QString &directory)
{
    QFile file(path(directory));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32768)
        return {};
    const auto object = QJsonDocument::fromJson(file.read(32769)).object();
    if (object.value("schema").toInt() != 1 || !object.value("caption").isString())
        return {};
    return CardMetadata{object.value("caption").toString().left(101),
                        object.value("avatarKey").toString().left(128),
                        QByteArray::fromBase64(object.value("png").toString().toLatin1())};
}

bool CardMetadata::write(const QString &directory) const
{
    const QJsonObject object{{"schema", 1}, {"caption", caption.left(101)}, {"avatarKey", avatarKey.left(128)},
                             {"png", QString::fromLatin1(png.toBase64())}};
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QSaveFile file(path(directory));
    return bytes.size() <= 32768 && file.open(QIODevice::WriteOnly)
           && file.write(bytes) == bytes.size() && file.commit();
}
}
