#pragma once
#include "SessionManager.hpp"
#include <QWidget>
#include <QPixmap>
#include <QSystemTrayIcon>

class QLabel;
class QScrollArea;
class QVBoxLayout;
class QPushButton;

namespace Acheron::Hub {

int runHub();
class HubWindow : public QWidget
{
public:
    explicit HubWindow(const QString &directory);
    void activate();
    void startup();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    Registry registry;
    SessionManager sessions;

    QTimer redraw;
    QVBoxLayout *cards;
    QScrollArea *scroll;
    QLabel *intro, *title, *notice;
    QPushButton *launchAll;
    QSystemTrayIcon *tray = nullptr;
    QHash<QString, QString> subtitles, stamps;
    QHash<QString, QPixmap> portraits;
    bool closingAll = false;
    int autoHeight = 0;
    void render();
    void addAccount();
    void preferences();
    void menu(const QString &id, QWidget *anchor);
    void quitAll();
    enum class LaunchGroup { All, Selected, Previous };
    QStringList eligible(LaunchGroup group) const;
    bool save(const QString &key, const QVariant &value);
};
}
