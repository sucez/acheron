#include "UpdateCenter.hpp"
#include "UpdateManager.hpp"
#include <QtWidgets>
namespace Acheron::Updates {
namespace {
QString stateTitle(UpdateManager::State state)
{
    using M = UpdateManager;
    switch (state) {
    case M::Idle: return QObject::tr("Ready to check");
    case M::Checking: return QObject::tr("Checking for updates…");
    case M::Current: return QObject::tr("Up to date");
    case M::Available: return QObject::tr("Update available");
    case M::Downloading: return QObject::tr("Downloading");
    case M::Verifying: return QObject::tr("Verifying downloaded file…");
    case M::Complete: return QObject::tr("Download complete");
    case M::Cancelled: return QObject::tr("Download cancelled");
    case M::Unavailable: return QObject::tr("Update unavailable");
    case M::VerificationFailed: return QObject::tr("Verification failed");
    case M::NetworkError: return QObject::tr("Network error");
    case M::StorageError: return QObject::tr("Storage error");
    }
    return {};
}
}
UpdateCenter::UpdateCenter(UpdateManager &updates, QWidget *parent) : QDialog(parent)
{
    using M = UpdateManager;
    setWindowTitle(tr("Update Center"));
    setMinimumWidth(370);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22,20,22,20);
    layout->setSpacing(10);
    auto *heading = new QLabel(tr("Updates"));
    heading->setObjectName("headline");
    layout->addWidget(heading);
    const auto &id = updates.client();
    layout->addWidget(new QLabel(tr("Installed: %1 · %2 · %3").arg(id.installed,id.buildChannel,id.arch)));
    if (id.buildChannel == "development")
        layout->addWidget(new QLabel(tr("Development build · manual checks only")));
    auto *channel = new QComboBox;
    channel->addItem(tr("Stable"),"stable");
    channel->addItem(tr("Preview"),"preview");
    auto *form = new QFormLayout;
    form->addRow(tr("Release channel"),channel);
    layout->addLayout(form);
    auto *automatic = new QCheckBox(tr("Automatically check for updates"));
    automatic->setEnabled(id.buildChannel != "development");
    layout->addWidget(automatic);
    auto *last = new QLabel;
    last->setObjectName("muted");
    layout->addWidget(last);
    auto *card = new QFrame;
    card->setObjectName("card");
    auto *panel = new QVBoxLayout(card);
    auto *state = new QLabel;
    state->setObjectName("cardName");
    auto *message = new QLabel;
    message->setWordWrap(true);
    message->setTextFormat(Qt::PlainText);
    auto *notes = new QLabel;
    notes->setOpenExternalLinks(true);
    auto *progress = new QProgressBar;
    panel->addWidget(state);
    panel->addWidget(message);
    panel->addWidget(notes);
    panel->addWidget(progress);
    layout->addWidget(card);
    auto *actions = new QHBoxLayout;
    auto *check = new QPushButton(tr("Check for updates"));
    check->setObjectName("primary");
    auto *download = new QPushButton(tr("Download"));
    auto *cancel = new QPushButton(tr("Cancel download"));
    auto *close = new QPushButton(tr("Close"));
    for (auto *button : {check, download, cancel, close}) actions->addWidget(button);
    layout->addLayout(actions);
    auto refresh = [=,&updates] {
        const auto phase = updates.state();
        bool active = phase == M::Downloading || phase == M::Verifying;
        QSignalBlocker blockChannel(channel), blockAuto(automatic);
        channel->setCurrentIndex(channel->findData(updates.channel()));
        channel->setEnabled(!active);
        automatic->setChecked(updates.automatic());
        const auto time = updates.lastCheck();
        last->setText(time ? tr("Last successful check: %1").arg(QLocale().toString(
            QDateTime::fromSecsSinceEpoch(time).toLocalTime(),QLocale::ShortFormat)) : tr("No successful check yet"));
        state->setText(stateTitle(phase));
        check->setEnabled(!active && phase != M::Checking);
        const auto &offer = updates.offer();
        QString detail = updates.message();
        if (offer && detail.isEmpty())
            detail = tr("Version %1 · %2 bytes").arg(offer->version).arg(offer->size);
        if (phase == M::Unavailable && detail.isEmpty()) detail = tr("No compatible public release was found.");
        message->setText(detail);
        message->setVisible(!detail.isEmpty());
        notes->setVisible(bool(offer));
        if (offer) notes->setText(QStringLiteral("<a style='color:#b5afff' href='%1'>Release notes on GitHub</a>").arg(offer->notes));
        download->setVisible(bool(offer) && !active && phase != M::Complete);
        download->setText(phase == M::Available ? tr("Download") : tr("Retry download"));
        cancel->setVisible(active);
        progress->setVisible(active);
        if (active && offer) {
            const auto count = updates.received();
            progress->setValue(int(100*count / offer->size));
            progress->setFormat(tr("%1 / %2 bytes · %p%").arg(count).arg(offer->size));
        }
    };
    auto save = [=,&updates] {
        if (!updates.preferences(channel->currentData().toString(),automatic->isChecked())) {
            refresh();
            QMessageBox::warning(this,windowTitle(),tr("Could not save update preferences."));
        }
    };
    connect(channel,QOverload<int>::of(&QComboBox::currentIndexChanged),this,save);
    connect(automatic,&QCheckBox::toggled,this,save);
    connect(check,&QPushButton::clicked,&updates,&M::check);
    connect(download,&QPushButton::clicked,&updates,&M::download);
    connect(cancel,&QPushButton::clicked,&updates,&M::cancelDownload);
    connect(close,&QPushButton::clicked,this,&QDialog::accept);
    connect(&updates,&M::changed,this,refresh);
    refresh();
}
}
