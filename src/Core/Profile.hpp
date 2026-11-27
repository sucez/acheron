#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QStringList>

namespace Acheron::Core::Profile {

inline const QString ApplicationPrefix = QStringLiteral("Acheron-Custom-");

inline QString canonicalName(const QString &input)
{
    static const QRegularExpression pattern(QStringLiteral("\\A[A-Za-z0-9][A-Za-z0-9_-]{0,31}\\z"));
    return pattern.match(input).hasMatch() ? input.toLower() : QString();
}

inline QStringList savedNames(const QDir &organizationDir)
{
    QStringList names;
    const auto entries = organizationDir.entryList({ApplicationPrefix + QLatin1Char('*')},
                                                   QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::CaseSensitive,
                                                   QDir::Name);
    for (const QString &entry : entries) {
        const QString name = entry.mid(ApplicationPrefix.size());
        if (!name.isEmpty() && canonicalName(name) == name)
            names.append(name);
    }
    return names;
}

inline QString tokenKey(const QString &accountId)
{
    const QString key = QStringLiteral("account_%1_token").arg(accountId);
#ifdef Q_OS_WIN
    // QtKeychain's Windows Credential Store backend uses only the key as TargetName.
    return QCoreApplication::applicationName() + QLatin1Char(':') + key;
#else
    return key;
#endif
}

// Called before QApplication so Windows can select the profile's font engine.
inline QString fromArguments(const QStringList &args)
{
    QString name = QStringLiteral("principal");
    bool seen = false;
    for (int i = 0; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (arg == QStringLiteral("--profile")) {
            if (seen || i + 1 == args.size())
                return {};
            name = canonicalName(args.at(++i));
        } else if (arg.startsWith(QStringLiteral("--profile="))) {
            if (seen)
                return {};
            name = canonicalName(arg.mid(10));
        } else {
            continue;
        }
        if (name.isEmpty())
            return {};
        seen = true;
    }
    return name;
}

} // namespace Acheron::Core::Profile
