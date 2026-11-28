#include "Manifest.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <array>

namespace Acheron::Updates {
namespace {
struct Version {
    std::array<int,3> core;
    QString preview;
};
std::optional<Version> parse(const QString &v)
{
    static const QRegularExpression pattern(QStringLiteral(R"(\A(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})(?:-preview\.(0|[1-9][0-9]*))?\z)"));
    auto match = pattern.match(v);
    if (!match.hasMatch()) return {};
    Version parsed{{}, match.captured(4)};
    for (int i = 0; i < 3; ++i) {
        parsed.core[i] = match.captured(i + 1).toInt();
        if (parsed.core[i] > 65535) return {};
    }
    return parsed;
}

// v1 is published with sorted keys. Compare compact representations before use:
// QJsonDocument alone would silently discard duplicate names.
QByteArray compactInput(const QByteArray &bytes)
{
    QByteArray compact;
    bool string = false, escaped = false;
    for (char c : bytes) {
        if (!string && (c == ' ' || c == '\n' || c == '\r' || c == '\t')) continue;
        compact += c;
        if (!escaped && c == '"') string = !string;
        escaped = string && !escaped && c == '\\';
    }
    return compact;
}
}

bool newer(const QString &candidate, const QString &installed, const QString &channel)
{
    auto a = parse(candidate), b = parse(installed);
    if (!a || !b || (channel != "stable" && channel != "preview")) return false;
    if (channel == "stable" && !a->preview.isEmpty()) return false;
    if (a->core != b->core) return a->core > b->core;
    if (a->preview.isEmpty() || b->preview.isEmpty())
        return a->preview.isEmpty() && !b->preview.isEmpty();
    if (a->preview.size() != b->preview.size()) return a->preview.size() > b->preview.size();
    return a->preview > b->preview;
}

QString releasePage(const QString &version)
{
    return "https://github.com/sucez/acheron/releases/tag/v" + version;
}

std::optional<Package> parseManifest(const QByteArray &bytes, const QString &candidate,
    const QString &installed, const QString &channel, const QString &os, const QString &arch)
{
    if (bytes.size() > ManifestLimit || !newer(candidate, installed, channel)
        || os != "windows" || arch != "x64") return {};
    auto doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject() || compactInput(bytes) != doc.toJson(QJsonDocument::Compact)) return {};
    auto obj = doc.object();
    const auto kind = candidate.contains('-') ? QStringLiteral("preview") : QStringLiteral("stable");
    const auto commit = obj.value("commit").toString();
    static const QRegularExpression sha1("\\A[0-9a-f]{40}\\z"), sha256("\\A[0-9a-f]{64}\\z");
    if (obj.size() != 6 || obj.value("schema_version") != QJsonValue(1)
        || obj.value("version").toString() != candidate || obj.value("channel").toString() != kind
        || !sha1.match(commit).hasMatch() || obj.value("release_notes").toString() != releasePage(candidate)) return {};
    const auto entries = obj.value("packages").toArray();
    if (entries.size() != 1) return {};
    auto asset = entries.first().toObject();
    auto name = "Acheron-Custom-" + candidate + "-windows-x64.zip";
    auto url = "https://github.com/sucez/acheron/releases/download/v" + candidate + "/" + name;
    auto hash = asset.value("sha256").toString();
    double size = asset.value("size").toDouble(-1);
    if (asset.size() != 6 || asset.value("architecture").toString() != arch || asset.value("os").toString() != os
        || asset.value("filename").toString() != name || asset.value("url").toString() != url
        || !sha256.match(hash).hasMatch() || size < 1 || size > PackageLimit || qint64(size) != size) return {};
    return Package{candidate, kind, commit, name, url, releasePage(candidate), QByteArray::fromHex(hash.toLatin1()), qint64(size)};
}
}
