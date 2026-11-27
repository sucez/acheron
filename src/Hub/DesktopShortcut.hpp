#pragma once
#include "Core/Profile.hpp"

namespace Acheron::Hub {
inline QString shortcutArguments(const QString &id)
{
    if (id.isEmpty() || Core::Profile::canonicalName(id) != id)
        return {};
    return QStringLiteral("--profile ") + id;
}
#ifdef Q_OS_WIN
QString createDesktopShortcut(const QString &id);
#endif
}
