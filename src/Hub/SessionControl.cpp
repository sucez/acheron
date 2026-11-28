#include "SessionControl.hpp"
#include "LocalControl.hpp"
#include "CardMetadata.hpp"
#include "Discord/CdnUrls.hpp"
#include "UI/Accounts/AccountsModel.hpp"
#include <QBuffer>
#include "Core/Session.hpp"
#include "UI/MainWindow.hpp"

#include <QApplication>
#include <QSaveFile>
#include <QTimer>

namespace Acheron::Hub {

int runSessionWindow(Core::Session &session, UI::MainWindow &window, const QString &directory, const QStringList &args)
{
    const auto marker = directory + "/restore-session";
    QSaveFile file(marker);
    if (file.open(QIODevice::WriteOnly))
        file.commit();

    auto *accounts = window.accountModel();
    auto preview = CardMetadata::read(directory);
    QUrl avatarUrl;
    const auto updateCard = [&] {
        CardMetadata card;
        avatarUrl.clear();
        if (const auto *account = accounts->singleAccount()) {
            card.caption = account->username.isEmpty() ? account->displayName.left(100)
                                                       : "@" + account->username.left(100);
            if (!account->avatar.isEmpty()) {
                card.avatarKey = account->id.toString() + "/" + account->avatar.left(80);
                avatarUrl = Discord::Cdn::userAvatar(account->id, account->avatar);
                if (preview && preview->avatarKey == card.avatarKey)
                    card.png = preview->png;
            }
        } else if (accounts->rowCount() > 1) {
            card.caption = QObject::tr("Multiple accounts");
        }
        if (!preview || card.caption != preview->caption || card.avatarKey != preview->avatarKey) {
            if (card.write(directory))
                preview = card;
        }
    };
    updateCard();
    // The context disconnects before captured local variables go out of scope.
    QObject previewContext;
    QObject::connect(accounts, &QAbstractItemModel::rowsInserted, &previewContext, updateCard);
    QObject::connect(accounts, &QAbstractItemModel::rowsRemoved, &previewContext, updateCard);
    QObject::connect(&session, &Core::Session::accountDetailsUpdated, &previewContext, updateCard);
    QObject::connect(session.getImageManager(), &Core::ImageManager::imageFetched, &previewContext,
                     [&](const QUrl &url, const QSize &, const QPixmap &pixmap) {
        if (!preview || !preview->png.isEmpty() || avatarUrl.isEmpty() || pixmap.isNull()
            || url.adjusted(QUrl::RemoveQuery) != avatarUrl.adjusted(QUrl::RemoveQuery))
            return;
        CardMetadata updated = *preview;
        QBuffer buffer(&updated.png);
        if (buffer.open(QIODevice::WriteOnly)
            && pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG")
            && updated.write(directory))
            preview = updated;
    });

    std::optional<Core::Snowflake> needsAuth;
    std::optional<Core::Snowflake> gatewayError;
    QObject controlContext;
    QObject::connect(&session, &Core::Session::gatewayFailed, &controlContext,
                     [&](Core::Snowflake id) { gatewayError = id; });
    QObject::connect(&session, &Core::Session::authenticationRequired, &controlContext,
                     [&](Core::Snowflake id) { needsAuth = id; });
    QObject::connect(&session, &Core::Session::connectionStateChanged, &controlContext,
                     [&](Core::Snowflake id, Core::ConnectionState state) {
        if (gatewayError == id && (state == Core::ConnectionState::Connecting || state == Core::ConnectionState::Connected))
            gatewayError.reset();
        if (needsAuth == id && (state == Core::ConnectionState::Connecting || state == Core::ConnectionState::Connected))
            needsAuth.reset();
    });
    bool preserve = false;
    QLocalServer server;
    if (!listen(server, directory, [&](const QByteArray &verb) {
            if (verb == "status") {
                for (const auto *client : session.getClients())
                    if (client->state() == Core::ConnectionState::Connecting)
                        return QByteArray("connecting");
                const auto *single = accounts->singleAccount();
                if (single && needsAuth == single->id)
                    return QByteArray("needs-auth");
                if (single && gatewayError == single->id)
                    return QByteArray("gateway-error");
                return QByteArray("window");
            }
            if (verb == "activate" || verb == "login") {
                window.showFromTray();
                QApplication::alert(&window);
                if (verb == "login")
                    window.openAccountsWindow();
            } else if (verb == "close" || verb == "suspend") {
                preserve = verb == "suspend";
                QTimer::singleShot(0, &window, &UI::MainWindow::quitFromTray);
            } else {
                return QByteArray("error");
            }
            return QByteArray("ok");
        }))
        qWarning("Could not start the local session control endpoint");

    window.show();
    QTimer::singleShot(0, &window, [&] {
        if (args.contains("--hub-add")) {
            window.openAccountsWindow();
        } else if (args.contains("--hub-login")) {
            QObject::connect(&session, &Core::Session::connectFailed, &window, &UI::MainWindow::openAccountsWindow);
            if (const auto *account = accounts->singleAccount())
                session.connectAccount(account->id);
            else {
                session.autoConnectAccounts();
                window.openAccountsWindow();
            }
        } else {
            session.autoConnectAccounts();
        }
    });
    const int result = qApp->exec();
    if (!preserve)
        QFile::remove(marker);
    return result;
}

}
