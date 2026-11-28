#include "Registry.hpp"
#include "Core/Profile.hpp"
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>

namespace Acheron::Hub {

Registry::Registry(const QString &hubDir)
    : parent(QFileInfo(hubDir).absoluteDir()),
      settings(QDir(hubDir).filePath("registry.ini"), QSettings::IniFormat)
{
    if (settings.allKeys().isEmpty() && settings.status() == QSettings::NoError) {
        settings.setValue("schema", 1);
        settings.sync();
    }
}

bool Registry::usable() const
{
    return settings.status() == QSettings::NoError && settings.value("schema").toInt() == 1;
}

QStringList Registry::profiles() const { return Core::Profile::savedNames(parent); }
QStringList Registry::ordered() const
{
    auto ids = profiles();
    QStringList order;
    for (const auto &id : option("order").toStringList())
        if (ids.removeOne(id))
            order << id;
    order.append(ids);
    const auto sort = option("sort").toString();
    std::stable_sort(order.begin(), order.end(), [&](const QString &a, const QString &b) {
        const bool favorite = option("favorite/" + a).toBool();
        if (favorite != option("favorite/" + b).toBool())
            return favorite;
        if (sort == "name")
            return QString::localeAwareCompare(name(a), name(b)) < 0;
        if (sort == "recent")
            return option("used/" + a).toLongLong() > option("used/" + b).toLongLong();
        return false;
    });
    return order;
}

bool Registry::moveTo(const QString &id, const QString &target)
{
    auto ids = ordered();
    const int position = ids.indexOf(target);
    if (position < 0 || id == target || option("favorite/" + id).toBool() != option("favorite/" + target).toBool()
        || !ids.removeOne(id))
        return false;
    ids.insert(position, id);
    return setOptions({{"order", ids}, {"sort", "custom"}});
}

QString Registry::directory(const QString &id) const { return parent.filePath(Core::Profile::ApplicationPrefix + id); }
QString Registry::name(const QString &id) const { return option("name/" + id, id).toString(); }

QString Registry::suggest() const
{
    for (int n = 1; ; ++n) {
        const QString id = QStringLiteral("account-%1").arg(n);
        if (!QFileInfo::exists(directory(id)))
            return id;
    }
}

bool Registry::create(const QString &id)
{
    return !id.isEmpty() && Core::Profile::canonicalName(id) == id && parent.mkdir(Core::Profile::ApplicationPrefix + id);
}

QVariant Registry::option(const QString &key, const QVariant &fallback) const
{
    return usable() ? settings.value(key, fallback) : fallback;
}

bool Registry::set(const QString &key, const QVariant &value)
{
    return setOptions({{key, value}});
}

bool Registry::setOptions(const QVariantMap &values)
{
    if (!usable())
        return false;
    for (auto it = values.begin(); it != values.end(); ++it)
        settings.setValue(it.key(), it.value());
    settings.sync();
    return settings.status() == QSettings::NoError;
}

QString Registry::summary(const QString &id) const
{
    // The caller caches this preview. Never construct AccountRepository or select credential fields.
    const QString path = QDir(directory(id)).filePath("acheron.sqlite");
    if (!QFileInfo::exists(path))
        return {};
    const QString connection = QUuid::createUuid().toString();
    QString label;
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
        db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=50");
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec("SELECT substr(username,1,100),substr(display_name,1,100) FROM accounts LIMIT 2") && query.next()) {
                label = query.value(0).toString();
                if (!label.isEmpty())
                    label.prepend('@');
                else
                    label = query.value(1).toString();
                if (query.next())
                    label = QObject::tr("Multiple accounts");
            }
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return label.simplified();
}

} // namespace Acheron::Hub
