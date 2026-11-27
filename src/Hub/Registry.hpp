#pragma once
#include <QDir>
#include <QSettings>

namespace Acheron::Hub {

class Registry
{
public:
    explicit Registry(const QString &hubDir);
    QStringList profiles() const;
    QString directory(const QString &id) const;
    QString suggest() const;
    bool create(const QString &id);
    QString name(const QString &id) const;
    QString summary(const QString &id) const;
    QVariant option(const QString &key, const QVariant &fallback = {}) const;
    bool set(const QString &key, const QVariant &value);
    bool setOptions(const QVariantMap &values);
    QString root() const { return parent.absolutePath(); }
    bool usable() const;

private:
    QDir parent;
    QSettings settings;
};

} // namespace Acheron::Hub
