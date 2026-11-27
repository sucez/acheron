#include "Hub/Registry.hpp"
#include "Hub/CardMetadata.hpp"
#include <QProcess>
#include <QSaveFile>
#include "Hub/LocalControl.hpp"

#include <QtTest>
#include <QLocalSocket>
#include <QTemporaryDir>

using namespace Acheron::Hub;

class TestHub : public QObject
{
    Q_OBJECT
private slots:
    void registry()
    {
        QTemporaryDir temp;
        const QString hub = temp.filePath("Acheron-Hub");
        QVERIFY(QDir().mkpath(hub));
        QVERIFY(QDir().mkdir(temp.filePath("Acheron-Custom-veteran")));
        QVERIFY(QDir().mkdir(temp.filePath("Acheron")));
        Registry first(hub), second(hub);
        QCOMPARE(first.profiles(), QStringList{"veteran"});
        auto id = first.suggest();
        QVERIFY(first.create(id));
        QVERIFY(!second.create(id));
        QVERIFY(id != second.suggest());
        QVERIFY(!first.create("../escape"));
        const auto location = first.directory(id);
        QVERIFY(first.set("name/" + id, "Studio"));
        QVERIFY(second.set("selected/" + id, true));
        Registry reopened(hub);
        QCOMPARE(reopened.name(id), QString("Studio"));
        QCOMPARE(reopened.directory(id), location);
        QVERIFY(reopened.option("selected/" + id).toBool());
        QVERIFY(reopened.profiles().contains(id));
    }

    void concurrentWriters()
    {
        QTemporaryDir temp;
        Registry registry(temp.path());
        QProcess left, right;
        const auto exe = QCoreApplication::applicationFilePath();
        left.start(exe, {"--writer", temp.path(), "left"});
        right.start(exe, {"--writer", temp.path(), "right"});
        QTRY_VERIFY(QFile::exists(temp.filePath("left")) && QFile::exists(temp.filePath("right")));
        QFile release(temp.filePath("start"));
        QVERIFY(release.open(QIODevice::WriteOnly));
        release.close();
        QTRY_VERIFY(left.state() == QProcess::NotRunning && right.state() == QProcess::NotRunning);
        QCOMPARE(left.exitStatus(), QProcess::NormalExit);
        QCOMPARE(right.exitStatus(), QProcess::NormalExit);
        QCOMPARE(left.exitCode(), 0);
        QCOMPARE(right.exitCode(), 0);
        Registry reopened(temp.path());
        QCOMPARE(reopened.option("name/left").toString(), QString("left"));
        QCOMPARE(reopened.option("name/right").toString(), QString("right"));
    }

    void previewSurvivesInterruptedWrite()
    {
        QTemporaryDir temp;
        CardMetadata original{"@synthetic", "test-key", {}};
        QVERIFY(original.write(temp.path()));
        {
            QSaveFile interrupted(CardMetadata::path(temp.path()));
            QVERIFY(interrupted.open(QIODevice::WriteOnly));
            interrupted.write("{");
        }
        auto restored = CardMetadata::read(temp.path());
        QVERIFY(restored);
        QCOMPARE(restored->caption, original.caption);
        QCOMPARE(restored->avatarKey, original.avatarKey);
        QFile file(CardMetadata::path(temp.path()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(32769, 'x'));
        file.close();
        QVERIFY(!CardMetadata::read(temp.path()));
    }

    void arrangement()
    {
        QTemporaryDir dir;
        Registry r(dir.filePath("Hub"));
        for (auto id : {"a", "b", "c"}) QVERIFY(r.create(id));
        QVERIFY(r.moveTo("c", "a"));
        Registry reopened(dir.filePath("Hub"));
        QCOMPARE(reopened.ordered(), QStringList({"c", "a", "b"}));
        QVERIFY(reopened.setOptions({{"favorite/b", true}, {"sort", "name"}}));
        QCOMPARE(reopened.ordered(), QStringList({"b", "a", "c"}));
        QVERIFY(reopened.setOptions({{"sort", "recent"}, {"used/c", 12}}));
        QCOMPARE(reopened.ordered(), QStringList({"b", "c", "a"}));
        QVERIFY(!reopened.moveTo("b", "c"));
    }

    void missingSchema()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("registry.ini"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[General]\nstartup=selected\n");
        file.close();
        Registry registry(dir.path());
        QVERIFY(!registry.usable());
        QVERIFY(!registry.set("startup", "hub"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("[General]\nstartup=selected\n"));
    }

    void damagedPreferences()
    {
        QTemporaryDir temp;
        const auto hub = temp.filePath("Acheron-Hub");
        QVERIFY(QDir().mkpath(hub));
        QVERIFY(QDir().mkdir(temp.filePath("Acheron-Custom-existing")));
        QFile broken(hub + "/registry.ini");
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("[unterminated\n");
        broken.close();
        Registry registry(hub);
        QVERIFY(!registry.usable());
        QCOMPARE(registry.profiles(), QStringList{"existing"});
        QVERIFY(!registry.set("default", "existing"));
        QVERIFY(broken.open(QIODevice::ReadOnly));
        QCOMPARE(broken.readAll(), QByteArray("[unterminated\n"));
    }

    void localProtocol()
    {
        QTemporaryDir temp;
        QLocalServer server;
        int handled = 0;
        QVERIFY(listen(server, temp.path(), [&handled](const QByteArray &verb) {
            ++handled;
            return verb == "status" ? QByteArray("window") : QByteArray("error");
        }));
        QByteArray reply;
        request(this, temp.path(), "status", [&](QByteArray value) { reply = value; });
        QTRY_COMPARE(reply, QByteArray("window"));
        QCOMPARE(handled, 1);
        QLocalSocket bad;
        bad.connectToServer(endpoint(temp.path()));
        QVERIFY(bad.waitForConnected());
        bad.write("2 status\n");
        QTRY_VERIFY(bad.canReadLine());
        QCOMPARE(bad.readAll(), QByteArray("1 error\n"));
        QCOMPARE(handled, 1);

        QLocalSocket large;
        large.connectToServer(endpoint(temp.path()));
        QVERIFY(large.waitForConnected());
        large.write(QByteArray(80, 'x'));
        QTRY_COMPARE(large.state(), QLocalSocket::UnconnectedState);
        QCOMPARE(handled, 1);
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() == 4 && args[1] == "--writer") {
        Registry registry(args[2]);
        QDir dir(args[2]);
        QFile ready(dir.filePath(args[3]));
        if (!ready.open(QIODevice::WriteOnly))
            return 1;
        ready.close();
        QElapsedTimer wait;
        wait.start();
        while (!QFile::exists(dir.filePath("start")) && wait.elapsed() < 5000)
            QThread::msleep(5);
        return registry.set("name/" + args[3], args[3]) ? 0 : 1;
    }
    TestHub test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_Hub.moc"
