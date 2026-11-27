#include "DesktopShortcut.hpp"
#include <QTemporaryDir>
#include <QFileInfo>
#include <QObject>
#include <QScopeGuard>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace Acheron::Hub {
QString createDesktopShortcut(const QString &id)
{
    const auto args = shortcutArguments(id).toStdWString();
    const auto failure = QObject::tr("Could not create the desktop shortcut.");
    if (args.empty()) return failure;
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) return failure;
    const auto uninitialize = qScopeGuard([&] { if (SUCCEEDED(init)) CoUninitialize(); });

    PWSTR desktop = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop))) return failure;
    QDir folder(QString::fromWCharArray(desktop));
    CoTaskMemFree(desktop);
    QTemporaryDir stage(folder.filePath(".acheron-XXXXXX"));
    if (!stage.isValid()) return failure;

    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) return failure;
    const auto releaseLink = qScopeGuard([&] { link->Release(); });
    const auto exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
    if (FAILED(link->SetPath(exe.c_str())) || FAILED(link->SetArguments(args.c_str()))
        || FAILED(link->SetIconLocation(exe.c_str(), 0))) return failure;
    IPersistFile *file = nullptr;
    if (FAILED(link->QueryInterface(IID_PPV_ARGS(&file)))) return failure;
    const auto releaseFile = qScopeGuard([&] { file->Release(); });
    const auto temp = QDir::toNativeSeparators(stage.filePath("profile.lnk")).toStdWString();
    if (FAILED(file->Save(temp.c_str(), TRUE))) return failure;
    const auto destination = QDir::toNativeSeparators(folder.filePath("Acheron - " + id + ".lnk")).toStdWString();
    if (!MoveFileW(temp.c_str(), destination.c_str())) {
        const DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS)
            return QObject::tr("A shortcut with this name already exists.");
        return failure;
    }
    return {};
}
}
