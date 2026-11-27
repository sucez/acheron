#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

namespace Acheron::Hub {

// A session publishes only the preview that is safe to display in the Hub.
struct CardMetadata
{
    QString caption;
    QString avatarKey;
    QByteArray png;
    static QString path(const QString &directory);
    static std::optional<CardMetadata> read(const QString &directory);
    bool write(const QString &directory) const;
};
}
