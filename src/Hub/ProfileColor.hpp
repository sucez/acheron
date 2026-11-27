#pragma once
#include <QStringList>

namespace Acheron::Hub {
inline const QStringList AvatarColors{"#4a416f", "#34565a", "#644351", "#435479"};
inline QString profileColor(const QString &id, int selection)
{
    return AvatarColors.value(selection, AvatarColors.at(qHash(id, 0) % AvatarColors.size()));
}
}
