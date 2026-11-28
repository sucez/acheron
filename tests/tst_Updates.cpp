#include "Updates/Manifest.hpp"
#include "Updates/Signature.hpp"
#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
using namespace Acheron::Updates;
class TestUpdates : public QObject {
    Q_OBJECT
private slots:
    void signatures() {
        const auto key = QByteArray::fromHex("8a6a628436bc616846d28fc40314e830ddc68adc25030693f1d1ecde76c810ab");
        const auto signature = QByteArray::fromHex("636435343435616631633132346164353264306265623265653161313764303636346461316662633865623961336131366135636635343564383839383834360afa3cf89caf78145f5cc2e94df02f769d4d3368ab0cc74aaced0c21bd89e09c69070a470d20731b2e374fbf6cac4f2a2a0a54eef3cd3dcc1d0d431f14cb5e6e08");
        QVERIFY(verifySignature("synthetic manifest",signature,{key}));
        QVERIFY(!verifySignature("modified",signature,{key}));
        QVERIFY(!verifySignature("synthetic manifest",signature,productionKeys()));
        QVERIFY(!verifySignature("synthetic manifest",signature,{QByteArray(32,'x')}));
        QVERIFY(!verifySignature("synthetic manifest",{},{key}));
        auto invalid = signature;
        invalid[128] = char(invalid.at(128) ^ 1);
        QVERIFY(!verifySignature("synthetic manifest",invalid,{key}));
    }
    void versions() {
        QVERIFY(newer("0.4.10","0.4.9","stable"));
        QVERIFY(newer("0.5.0-preview.2","0.5.0-preview.1","preview"));
        QVERIFY(newer("0.5.0","0.5.0-preview.2","stable"));
        QVERIFY(!newer("0.5.0-preview.2","0.4.9","stable"));
        for (auto bad : {"0.4.8","0.4.9","00.5.0","0.5","65536.0.0","0.5.0-preview.01"})
            QVERIFY(!newer(bad,"0.4.9","preview"));
    }
    void manifest() {
        QString version="0.4.10", name="Acheron-Custom-0.4.10-windows-x64.zip";
        QJsonObject asset{{"architecture","x64"},{"filename",name},{"os","windows"},{"sha256",QString(64,'a')},
            {"size",4},{"url","https://github.com/sucez/acheron/releases/download/v0.4.10/"+name}};
        QJsonObject doc{{"channel","stable"},{"commit",QString(40,'1')},{"packages",QJsonArray{asset}},
            {"release_notes",releasePage(version)},{"schema_version",1},{"version",version}};
        auto bytes=QJsonDocument(doc).toJson(QJsonDocument::Compact);
        auto valid=[&](QByteArray b){return parseManifest(b,version,"0.4.9","stable","windows","x64").has_value();};
        QVERIFY(valid(bytes));
        QVERIFY(!valid(QByteArray(ManifestLimit+1,' ')+bytes));
        for (auto b : {QByteArray(bytes).replace("\"size\":4","\"size\":\"4\""),QByteArray(bytes).replace("\"size\":4","\"size\":5,\"size\":4"),
                       QByteArray(bytes).replace("github.com","example.org"),QByteArray(bytes).replace(".zip",".exe"),
                       QByteArray(bytes).replace("\"x64\"","\"arm64\""),QByteArray(bytes).replace("\"schema_version\":1,","")})
            QVERIFY(!valid(b));
    }
};
QTEST_GUILESS_MAIN(TestUpdates)
#include "tst_Updates.moc"
