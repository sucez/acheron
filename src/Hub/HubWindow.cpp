#include "HubWindow.hpp"
#include "AccountCard.hpp"
#include "DesktopShortcut.hpp"
#include "Core/Profile.hpp"
#include "LocalControl.hpp"
#include "CardMetadata.hpp"
#include <QBuffer>
#include <QImageReader>

#include <QtWidgets>
#include <QLockFile>
#include <QLocalSocket>
#include <QThread>

namespace Acheron::Hub {
namespace {

const char Theme[] = R"(
QWidget {background:#171820;color:#f5f4fa;font-family:"Segoe UI","DejaVu Sans",sans-serif;font-size:13px}
QLabel {background:transparent}
QLabel#brand {font-size:14px;font-weight:bold;letter-spacing:2px}
QLabel#hub {color:#aea7ff;font-weight:bold;letter-spacing:2px}
QLabel#headline {font-size:28px;font-weight:bold}
QLabel#muted {color:#aaaabb}
QLabel#cardName {font-size:15px;font-weight:bold}
QLabel#notice {color:#f2b2ad}
QFrame#card {background:#242631;border:1px solid #393b49;border-radius:10px}
QFrame#card:hover {border-color:#686782}
QPushButton,QToolButton {background:#323440;border:1px solid #454754;border-radius:6px;padding:7px 12px;color:#f5f4fa}
QPushButton:hover,QToolButton:hover {background:#414452}
QPushButton:focus,QToolButton:focus {border-color:#aaa4ff}
QPushButton#primary {background:#635bd2;border-color:#736ce0;color:white;font-weight:bold}
QPushButton#primary:hover {background:#7068da}
QPushButton#running {color:#a3e6cf;border-color:#426257;background:#293b36}
QPushButton#failed {color:#ffb7b9;border-color:#715055}
QPushButton:disabled {color:#9696a5;background:#2c2e38;border-color:#393b49}
QToolButton {padding:5px 8px}
QScrollArea {border:0}
QLineEdit,QComboBox,QListWidget {background:#242631;border:1px solid #484b5b;padding:6px;border-radius:4px}
QMenu {background:#242631;border:1px solid #484b5b;padding:4px}
QMenu::item {padding:6px 18px}
QMenu::item:selected {background:#484369}
QListWidget::indicator {width:14px;height:14px;border:1px solid #888b9d;border-radius:3px;background:#292b37}
QListWidget::indicator:checked {background:#8a83f1;border-color:#b6b2ff}
)";

QLabel *label(const QString &text, const char *style = nullptr)
{
    auto *widget = new QLabel;
    widget->setTextFormat(Qt::PlainText);
    widget->setText(text);
    if (style)
        widget->setObjectName(style);
    return widget;
}

// Qt's plain-text labels would otherwise make very long account names widen the window.
class NameLabel : public QLabel {
public:
    explicit NameLabel(const QString &value) : full(value) {
        setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setToolTip(full.toHtmlEscaped());
        setText(full);
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        QLabel::resizeEvent(event);
        setText(fontMetrics().elidedText(full, Qt::ElideRight, width()));
    }
private:
    QString full;
};

const QStringList AvatarColors{"#4a416f", "#34565a", "#644351", "#435479"};

void foreground(QWidget &window)
{
    window.setWindowState(window.windowState() & ~Qt::WindowMinimized);
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::alert(&window);
}
}

int runHub()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty() || !QDir().mkpath(dir))
        return 1;
    QLockFile lock(dir + "/hub.lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        for (int attempt = 0; attempt < 15; ++attempt) {
            QLocalSocket peer;
            peer.connectToServer(endpoint(dir));
            if (peer.waitForConnected(100)) {
                peer.write("1 activate\n");
                peer.flush();
                if (peer.bytesAvailable() || peer.waitForReadyRead(800))
                    return peer.readAll() == "1 ok\n" ? 0 : 1;
            }
            if (lock.tryLock(0))
                break;
            QThread::msleep(50);
        }
        if (!lock.isLocked())
            return 1;
    }
    HubWindow window(dir);
    QLocalServer server;
    if (!listen(server, dir, [&window](const QByteArray &verb) {
            if (verb != "activate")
                return QByteArray("error");
            window.activate();
            return QByteArray("ok");
        }))
        return 1;
    window.show();
    window.startup();
    return qApp->exec();
}

HubWindow::HubWindow(const QString &directory) : registry(directory), sessions(registry)
{
    setWindowTitle(tr("Acheron Hub"));
    setWindowIcon(QIcon(":/acheron.svg"));
    setStyleSheet(QString::fromLatin1(Theme));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(10);
    auto *header = new QHBoxLayout;
    auto *logo = label({});
    logo->setPixmap(windowIcon().pixmap(32, 32));
    header->addWidget(logo);
    header->addWidget(label("ACHERON", "brand"));
    header->addWidget(label("HUB", "hub"));
    header->addStretch();
    auto *settings = new QPushButton(tr("Settings"));
    settings->setShortcut(QKeySequence("Ctrl+,"));
    auto *find = new QToolButton;
    find->setText(tr("Find"));
    find->setShortcut(QKeySequence::Find);
    find->setToolTip(tr("Find a profile or username (Ctrl+F)"));
    header->addWidget(find);
    header->addWidget(settings);
    layout->addLayout(header);
    layout->addSpacing(8);
    title = label({}, "headline");
    intro = label({}, "muted");
    intro->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(intro);
    search = new QLineEdit;
    search->setPlaceholderText(tr("Find a profile or username"));
    search->setClearButtonEnabled(true);
    layout->addWidget(search);
    search->hide();
    connect(find, &QToolButton::clicked, this, [this] {
        search->setVisible(!search->isVisible());
        if (search->isVisible()) search->setFocus();
        else search->clear();
    });
    connect(search, &QLineEdit::textChanged, this, &HubWindow::render);
    scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *body = new QWidget;
    cards = new QVBoxLayout(body);
    cards->setContentsMargins(0, 5, 0, 0);
    cards->setAlignment(Qt::AlignTop);
    cards->setSpacing(8);
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);
    notice = label({}, "notice");
    notice->setWordWrap(true);
    notice->hide();
    layout->addWidget(notice);
    auto *actions = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add an account"));
    add->setObjectName("primary");
    add->setShortcut(QKeySequence("Ctrl+N"));
    launchAll = new QPushButton(tr("Launch all accounts"));
    launchAll->setShortcut(QKeySequence("Ctrl+Shift+L"));
    actions->addWidget(add);
    actions->addWidget(launchAll);
    actions->addStretch();
    layout->addLayout(actions);
    connect(settings, &QPushButton::clicked, this, &HubWindow::preferences);
    connect(add, &QPushButton::clicked, this, &HubWindow::addAccount);
    connect(launchAll, &QPushButton::clicked, this, [this] { sessions.enqueue(eligible(LaunchGroup::All)); });
    redraw.setInterval(60);
    redraw.setSingleShot(true);
    connect(&redraw, &QTimer::timeout, this, &HubWindow::render);
    connect(&sessions, &SessionManager::changed, this, [this] { redraw.start(); });
    connect(&sessions, &SessionManager::problem, this, [this](const QString &msg) { notice->setText(msg); notice->show(); });
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray = new QSystemTrayIcon(windowIcon(), this);
        auto *context = new QMenu(this);
        context->addAction(tr("Open Acheron Hub"), this, &HubWindow::activate);
        context->addAction(tr("Quit all sessions"), this, &HubWindow::quitAll);
        context->addAction(tr("Quit Hub"), qApp, &QApplication::quit);
        tray->setContextMenu(context);
        tray->setToolTip(windowTitle());
        connect(tray, &QSystemTrayIcon::activated, this, [this](auto reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                activate();
        });
        tray->show();
    }
    render();
    if (!registry.usable()) {
        notice->setText(tr("Hub preferences could not be loaded. Your profile data is still available."));
        notice->show();
    }
}

void HubWindow::activate() { foreground(*this); }

bool HubWindow::save(const QString &key, const QVariant &value)
{
    if (registry.set(key, value))
        return true;
    QMessageBox::warning(this, windowTitle(), tr("Could not save this preference."));
    return false;
}

void HubWindow::render()
{
    const int scrollPosition = scroll->verticalScrollBar()->value();
    const auto *focus = QApplication::focusWidget();
    const QString actionToFocus = focus ? focus->property("hubAction").toString() : QString();
    QWidget *nextFocus = nullptr;
    while (auto *item = cards->takeAt(0)) {
        item->widget()->hide();
        item->widget()->deleteLater();
        delete item;
    }
    const auto density = registry.option("density").toString();
    const int padding = density == "compact" ? 7 : density == "spacious" ? 18 : 12;
    cards->setSpacing(padding / 2 + 2);
    int visible = 0;
    for (const QString &id : registry.ordered()) {
        const auto state = sessions.state(id);
        if (registry.option("archived/" + id).toBool() && state == SessionManager::Closed)
            continue;
        ++visible;
        const auto directory = registry.directory(id);
        const QFileInfo metadata(CardMetadata::path(directory));
        const auto stamp = QString::number(metadata.size()) + '/' + QString::number(metadata.lastModified().toMSecsSinceEpoch());
        if (stamps.value(id) != stamp) {
            stamps[id] = stamp;
            portraits.remove(id);
            const auto preview = CardMetadata::read(directory);
            subtitles[id] = preview ? preview->caption : registry.summary(id);
            if (preview && !preview->png.isEmpty()) {
                QBuffer buffer;
                buffer.setData(preview->png);
                buffer.open(QIODevice::ReadOnly);
                QImageReader reader(&buffer, "png");
                const QSize size = reader.size();
                if (size.isValid() && size.width() <= 64 && size.height() <= 64) {
                    const auto source = reader.read();
                    if (!source.isNull()) {
                        QPixmap circle(64, 64);
                        circle.fill(Qt::transparent);
                        QPainter painter(&circle);
                        painter.setRenderHint(QPainter::Antialiasing);
                        QPainterPath clip;
                        clip.addEllipse(0, 0, 64, 64);
                        painter.setClipPath(clip);
                        painter.drawImage(circle.rect(), source);
                        painter.end();
                        portraits[id] = circle;
                    }
                }
            }
        }
        const auto display = registry.name(id);
        const auto query = search->text().trimmed();
        if (!id.contains(query, Qt::CaseInsensitive) && !display.contains(query, Qt::CaseInsensitive)
            && !subtitles.value(id).contains(query, Qt::CaseInsensitive))
            continue;
        auto *card = new AccountCard(id, [this](const QString &source, const QString &target) {
            if (registry.moveTo(source, target)) redraw.start();
        });
        card->setObjectName("card");
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(12, padding, 10, padding);
        const int savedColor = registry.option("color/" + id, -1).toInt();
        const auto color = AvatarColors.value(savedColor, AvatarColors.at(qHash(id, 0) % AvatarColors.size()));
        auto *avatar = label(display.left(1).toUpper());
        if (portraits.contains(id))
            avatar->setPixmap(portraits.value(id).scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        avatar->setFixedSize(36, 36);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet("background:" + color + ";border-radius:18px;font-weight:bold;font-size:16px");
        avatar->setToolTip(tr("Drag to reorder within favorites or other accounts."));
        row->addWidget(avatar);
        auto *text = new QVBoxLayout;
        text->setSpacing(3);
        auto *name = new NameLabel(registry.option("favorite/" + id).toBool() ? QStringLiteral("★ ") + display : display);
        name->setObjectName("cardName");
        text->addWidget(name);
        const QString subtitle = subtitles.value(id);
        auto *sub = new NameLabel(subtitle.isEmpty() ? tr("Profile · %1").arg(id) : subtitle);
        sub->setObjectName("muted");
        text->addWidget(sub);
        row->addLayout(text, 1);
        QString action;
        switch (state) {
        case SessionManager::Launching: action = tr("Launching…"); break;
        case SessionManager::Connecting: action = tr("Connecting…"); break;
        case SessionManager::NeedsAuth: action = tr("Reconnect"); break;
        case SessionManager::Open: action = tr("Switch to window"); break;
        case SessionManager::Locked: action = tr("In use"); break;
        case SessionManager::Failed: action = tr("Retry"); break;
        default: action = tr("Login");
        }
        auto *button = new QPushButton(action);
        if (state == SessionManager::Open || state == SessionManager::Connecting)
            button->setObjectName("running");
        else if (state == SessionManager::Failed || state == SessionManager::NeedsAuth)
            button->setObjectName("failed");
        button->setEnabled(state != SessionManager::Launching);
        connect(button, &QPushButton::clicked, this, [this,id] { sessions.open(id); });
        row->addWidget(button);
        auto *more = new QToolButton;
        more->setText(QStringLiteral("⋯"));
        more->setToolTip(tr("Account actions"));
        more->setAccessibleName(tr("Actions for %1").arg(display));
        connect(more, &QToolButton::clicked, this, [this,id,more] { menu(id, more); });
        button->setProperty("hubAction", "open/" + id);
        more->setProperty("hubAction", "menu/" + id);
        if (actionToFocus == "open/" + id)
            nextFocus = button;
        else if (actionToFocus == "menu/" + id)
            nextFocus = more;
        row->addWidget(more);
        cards->addWidget(card);
    }
    title->setText(visible ? tr("Welcome back.") : tr("Welcome to Acheron."));
    intro->setText(visible ? tr("Choose an account to start chatting.")
                           : tr("Add your first account to open a chat window. Each account has its own space."));
    if (visible && cards->count() == 0) intro->setText(tr("No matching accounts."));
    launchAll->setVisible(visible > 1);
    const int targetHeight = qMin(225 + visible * (44 + 2 * padding + cards->spacing()), 650);
    if (autoHeight == 0 || height() == autoHeight)
        resize(autoHeight ? width() : 480, targetHeight);
    autoHeight = targetHeight;
    if (nextFocus)
        nextFocus->setFocus(Qt::OtherFocusReason);
    scroll->verticalScrollBar()->setValue(scrollPosition);
}

void HubWindow::addAccount()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add an account"));
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(10);
    layout->addWidget(label(tr("Add an account"), "headline"));
    auto *description = label(tr("Choose a profile name, then sign in from its chat window."), "muted");
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *name = new QLineEdit(registry.suggest());
    auto *field = label(tr("Profile name"));
    field->setBuddy(name);
    layout->addWidget(field);
    layout->addWidget(name);
    auto *hint = label(tr("1–32 letters, numbers, hyphens or underscores. Start with a letter or number."), "muted");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto *create = buttons->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    create->setObjectName("primary");
    create->setDefault(true);
    layout->addWidget(buttons);
    const auto validate = [&] { create->setEnabled(!Core::Profile::canonicalName(name->text()).isEmpty()); };
    connect(name, &QLineEdit::textChanged, &dialog, validate);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (registry.create(Core::Profile::canonicalName(name->text())))
            dialog.accept();
        else {
            hint->setText(tr("Could not create this profile. Choose an unused name and check folder permissions."));
            name->setFocus();
            name->selectAll();
        }
    });
    name->selectAll();
    name->setFocus();
    dialog.resize(360, dialog.sizeHint().height());
    if (dialog.exec() != QDialog::Accepted)
        return;
    const auto id = Core::Profile::canonicalName(name->text());
    sessions.refresh();
    sessions.open(id, true);
}

QStringList HubWindow::eligible(LaunchGroup group) const
{
    QStringList ids;
    for (const auto &id : registry.profiles()) {
        if (registry.option("archived/" + id).toBool())
            continue;
        if (group == LaunchGroup::All && registry.option("exclude/" + id).toBool())
            continue;
        if (group == LaunchGroup::Selected && !registry.option("selected/" + id).toBool())
            continue;
        if (group == LaunchGroup::Previous && !QFileInfo::exists(registry.directory(id) + "/restore-session"))
            continue;
        ids << id;
    }
    return ids;
}

void HubWindow::startup()
{
    const auto mode = registry.option("startup").toString();
    if (mode == "selected")
        sessions.enqueue(eligible(LaunchGroup::Selected));
    else if (mode == "restore")
        sessions.enqueue(eligible(LaunchGroup::Previous));
    else if (mode == "default") {
        const QString id = registry.option("default").toString();
        if (registry.profiles().contains(id) && !registry.option("archived/" + id).toBool()) {
            sessions.open(id);
            showMinimized();
        }
    }
}

void HubWindow::menu(const QString &id, QWidget *anchor)
{
    QMenu actions(this);
    actions.addAction(tr("Open account"), this, [this,id] { sessions.open(id); });
    auto *close = actions.addAction(tr("Close session"), this, [this,id] { sessions.close(id, false); });
    close->setEnabled(sessions.controllable(id));
    actions.addSeparator();
    auto *favorite = actions.addAction(tr("Favorite"));
    favorite->setCheckable(true);
    favorite->setChecked(registry.option("favorite/" + id).toBool());
    connect(favorite, &QAction::triggered, this, [this,id](bool on) {
        if (save("favorite/" + id, on)) redraw.start();
    });
    QStringList peers;
    for (const auto &other : registry.ordered())
        if (!registry.option("archived/" + other).toBool() &&
            registry.option("favorite/" + other).toBool() == favorite->isChecked())
            peers << other;
    for (int step : {-1, 1}) {
        const auto target = peers.value(peers.indexOf(id) + step);
        auto *action = actions.addAction(step < 0 ? tr("Move up") : tr("Move down"), this, [this,id,target] {
            if (registry.moveTo(id, target)) redraw.start();
        });
        action->setEnabled(!target.isEmpty());
    }
    actions.addAction(tr("Rename profile…"), this, [this,id] {
        bool ok = false;
        const auto name = QInputDialog::getText(this, tr("Rename profile"), tr("Display name"), QLineEdit::Normal,
                                                registry.name(id), &ok).simplified().left(100);
        if (ok && !name.isEmpty() && save("name/" + id, name))
            render();
    });
    auto *colors = actions.addMenu(tr("Profile color"));
    const QStringList names{tr("Violet"), tr("Teal"), tr("Rose"), tr("Blue")};
    for (int i = 0; i < names.size(); ++i) {
        QPixmap sample(14, 14);
        sample.fill(QColor(AvatarColors.at(i)));
        colors->addAction(QIcon(sample), names.at(i), this, [this,id,i] {
            if (save("color/" + id, i))
                render();
        });
    }
    auto *include = actions.addAction(tr("Include in launch all"));
    include->setCheckable(true);
    include->setChecked(!registry.option("exclude/" + id).toBool());
    connect(include, &QAction::triggered, this, [this,id](bool checked) { save("exclude/" + id, !checked); });
    actions.addAction(tr("Set as default"), this, [this,id] { save("default", id); });
#ifdef Q_OS_WIN
    actions.addAction(tr("Create desktop shortcut"), this, [this,id] {
        const auto error = createDesktopShortcut(id);
        if (!error.isEmpty())
            QMessageBox::warning(this, windowTitle(), error);
    });
#endif
    auto *archive = actions.addAction(tr("Archive profile"), this, [this,id] {
        if (save("archived/" + id, true))
            render();
    });
    archive->setEnabled(!sessions.busy(id));
    // Align to the right edge of the action button so the menu stays over its card.
    actions.exec(anchor->mapToGlobal(QPoint(anchor->width() - actions.sizeHint().width(), anchor->height())));
}

void HubWindow::preferences()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Hub settings"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *density = new QComboBox;
    density->addItem(tr("Compact"), "compact");
    density->addItem(tr("Comfortable"), "comfortable");
    density->addItem(tr("Spacious"), "spacious");
    const int index = density->findData(registry.option("density"));
    density->setCurrentIndex(index < 0 ? 1 : index);
    form->addRow(tr("Card density"), density);
    auto *sort = new QComboBox;
    sort->addItem(tr("Custom order"), "custom");
    sort->addItem(tr("Name"), "name");
    sort->addItem(tr("Last used"), "recent");
    sort->setCurrentIndex(qMax(0, sort->findData(registry.option("sort"))));
    form->addRow(tr("Sort by"), sort);
    auto *startup = new QComboBox;
    startup->addItem(tr("Open Hub only"), "hub");
    startup->addItem(tr("Restore previous sessions"), "restore");
    startup->addItem(tr("Launch selected accounts"), "selected");
    startup->addItem(tr("Launch default account"), "default");
    startup->setCurrentIndex(qMax(0, startup->findData(registry.option("startup"))));
    form->addRow(tr("On startup"), startup);
    auto *defaultId = new QComboBox;
    const auto ids = registry.profiles();
    for (const auto &id : ids)
        if (!registry.option("archived/" + id).toBool())
            defaultId->addItem(registry.name(id), id);
    defaultId->setCurrentIndex(qMax(0, defaultId->findData(registry.option("default"))));
    form->addRow(tr("Default account"), defaultId);
    auto *close = new QComboBox;
    close->addItem(tr("Close Hub only"), "hub");
    if (tray)
        close->addItem(tr("Minimize to tray"), "tray");
    close->addItem(tr("Quit all sessions"), "all");
    close->setCurrentIndex(qMax(0, close->findData(registry.option("close"))));
    form->addRow(tr("On close"), close);
    layout->addLayout(form);
    auto *selected = new QListWidget;
    for (const auto &id : ids) {
        auto *item = new QListWidgetItem(registry.name(id), selected);
        item->setData(Qt::UserRole, id);
        item->setCheckState(registry.option("selected/" + id).toBool() ? Qt::Checked : Qt::Unchecked);
    }
    layout->addWidget(label(tr("Accounts for selected startup"), "muted"));
    layout->addWidget(selected);
    auto *archived = new QComboBox;
    for (const auto &id : ids)
        if (registry.option("archived/" + id).toBool())
            archived->addItem(registry.name(id), id);
    if (archived->count()) {
        auto *restore = new QPushButton(tr("Unarchive"));
        auto *bar = new QHBoxLayout;
        bar->addWidget(archived); bar->addWidget(restore);
        layout->addLayout(bar);
        connect(restore, &QPushButton::clicked, &dialog, [this,archived] {
            if (archived->currentIndex() >= 0 && save("archived/" + archived->currentData().toString(), false)) {
                archived->removeItem(archived->currentIndex());
                render();
            }
        });
    } else {
        delete archived;
    }
    auto *quit = new QPushButton(tr("Quit all sessions…"));
    layout->addWidget(quit);
    connect(quit, &QPushButton::clicked, &dialog, [&] { dialog.reject(); quitAll(); });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        QVariantMap values{{"density", density->currentData()}, {"sort", sort->currentData()}, {"default", defaultId->currentData()}, {"startup", startup->currentData()},
                           {"close", close->currentData()}};
        for (int i = 0; i < selected->count(); ++i) {
            const auto *item = selected->item(i);
            values.insert("selected/" + item->data(Qt::UserRole).toString(), item->checkState() == Qt::Checked);
        }
        if (!registry.setOptions(values))
            QMessageBox::warning(this, windowTitle(), tr("Could not save Hub preferences."));
        render();
    }
}

void HubWindow::quitAll()
{
    if (closingAll)
        return;
    QStringList active;
    for (const auto &id : registry.profiles())
        if (sessions.busy(id))
            active << id;
    if (!active.isEmpty() && QMessageBox::question(this, windowTitle(), tr("Quit the Hub and close %1 session(s)?").arg(active.size())) != QMessageBox::Yes)
        return;
    sessions.cancelQueue();
    closingAll = true;
    // Wait for orderly exits. Keep the Hub available if a session cannot be reached.
    auto *wait = new QTimer(this);
    const auto end = QDateTime::currentMSecsSinceEpoch() + 18000;
    connect(wait, &QTimer::timeout, this, [this,end,wait, requested = QSet<QString>{}]() mutable {
        bool done = true;
        for (const auto &id : registry.profiles()) {
            if (!sessions.busy(id))
                continue;
            done = false;
            if (!requested.contains(id) && sessions.controllable(id)) {
                requested.insert(id);
                sessions.close(id, true);
            }
        }
        if (done)
            qApp->quit();
        else if (QDateTime::currentMSecsSinceEpoch() >= end) {
            wait->stop();
            wait->deleteLater();
            closingAll = false;
            notice->setText(tr("Some sessions are still open. Close them in their own windows."));
            notice->show();
        }
    });
    wait->start(200);
}

void HubWindow::closeEvent(QCloseEvent *event)
{
    const auto mode = registry.option("close").toString();
    if (mode == "all") {
        event->ignore();
        quitAll();
    } else if (mode == "tray" && tray) {
        event->ignore();
        hide();
    } else {
        event->accept();
    }
}
}
