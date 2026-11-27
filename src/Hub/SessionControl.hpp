#pragma once
#include <QStringList>
namespace Acheron::Core { class Session; }
namespace Acheron::UI { class MainWindow; }
namespace Acheron::Hub {
int runSessionWindow(Core::Session &session, UI::MainWindow &window, const QString &directory, const QStringList &args);
}
