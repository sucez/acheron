#include "UpdateManager.hpp"
#include "Signature.hpp"
#include <QDateTime>
#include <memory>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
namespace Acheron::Updates {
UpdateManager::UpdateManager(Hub::Registry &r, QNetworkAccessManager &network, ClientIdentity id)
    : registry(r), identity(std::move(id)), transfer(network) {}
QString UpdateManager::channel() const {
    return registry.option("updates/channel").toString() == "preview" ? "preview" : "stable";
}
bool UpdateManager::automatic() const { return registry.option("updates/auto",false).toBool(); }
qint64 UpdateManager::lastCheck() const { return registry.option("updates/last/" + channel()).toLongLong(); }

bool UpdateManager::preferences(QString selected, bool enabled)
{
    if (selected != "preview" && selected != "stable") return false;
    bool switching = selected != channel();
    if (!registry.setOptions({{"updates/channel", selected},{"updates/auto",enabled}})) return false;
    if (switching) {
        transfer.cancel();
        package.reset();
        set(Idle);
    } else emit changed();
    return true;
}

void UpdateManager::startup()
{
    if (automatic() && identity.buildChannel != "development"
        && QDateTime::currentSecsSinceEpoch() - lastCheck() >= 86400) check();
}
void UpdateManager::set(State state, QString message)
{
    phase = state;
    detail = std::move(message);
    emit changed();
}
void UpdateManager::success(State state)
{
    registry.set("updates/last/" + channel(), QDateTime::currentSecsSinceEpoch());
    set(state);
}

void UpdateManager::check()
{
    transfer.cancel();
    package.reset();
    manifest.clear();
    signature.clear();
    best.clear();
    sawCompatible = false;
    if (identity.os != "windows" || identity.arch != "x64") {
        set(Unavailable,tr("No distribution package for this platform."));
        return;
    }
    set(Checking);
    list(1);
}

void UpdateManager::fetchSmall(const QUrl &url, int limit, bool asset, std::function<void(QByteArray)> next)
{
    auto bytes = std::make_shared<QByteArray>();
    transfer.get(url,limit,asset,[bytes](const QByteArray &chunk){ bytes->append(chunk); return true; },
        [this,asset,next,bytes](QString error,int status) {
            if (error.isEmpty()) next(*bytes);
            else if (asset && status == 404) set(Unavailable,tr("Signed release metadata is missing."));
            else if (status == 200) set(VerificationFailed,error);
            else set(NetworkError,error);
        });
}

void UpdateManager::list(int page)
{
    auto url = QUrl(QStringLiteral("https://api.github.com/repos/sucez/acheron/releases?per_page=20&page=%1").arg(page));
    fetchSmall(url,1024*1024,false,[this,page](QByteArray bytes) {
        const auto doc = QJsonDocument::fromJson(bytes);
        if (!doc.isArray() || doc.array().size() > 20) { set(NetworkError,tr("Invalid GitHub release list.")); return; }
        for (auto value : doc.array()) {
            auto obj = value.toObject();
            if (!obj.value("draft").isBool() || !obj.value("prerelease").isBool() || !obj.value("tag_name").isString()) {
                set(NetworkError,tr("Invalid GitHub release record.")); return;
            }
            auto tag = obj.value("tag_name").toString();
            if (obj.value("draft").toBool() || !tag.startsWith('v')) continue;
            auto v = tag.mid(1);
            if (obj.value("prerelease").toBool() != v.contains("-preview.")) continue;
            if (!newer(v,"0.0.0-preview.0",channel())) continue;
            sawCompatible = true;
            if (newer(v,best.isEmpty() ? identity.installed : best,channel())) best = v;
        }
        if (doc.array().size() == 20) {
            if (page == 25) set(Unavailable,tr("Release list exceeds the search limit."));
            else list(page+1);
        } else if (best.isEmpty()) {
            success(sawCompatible ? Current : Unavailable);
        } else authenticate();
    });
}

void UpdateManager::authenticate()
{
    auto base = "https://github.com/sucez/acheron/releases/download/v" + best + "/";
    fetchSmall(QUrl(base+"manifest.json"),ManifestLimit,true,[this,base](QByteArray bytes) {
        manifest = std::move(bytes);
        fetchSmall(QUrl(base+"manifest.json.sig"),129,true,[this](QByteArray envelope) {
            if (!verifySignature(manifest,envelope,identity.keys)) {
                set(VerificationFailed,tr("The manifest has no valid signature from a trusted key.")); return;
            }
            package = parseManifest(manifest,best,identity.installed,channel(),identity.os,identity.arch);
            if (!package) { set(VerificationFailed,tr("Incompatible or invalid signed manifest.")); return; }
            signature = std::move(envelope);
            success(Available);
        });
    });
}
}
