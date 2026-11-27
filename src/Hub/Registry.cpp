#include "Registry.hpp"
#include "Core/Profile.hpp"
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

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
