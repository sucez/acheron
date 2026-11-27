#include "Core/Profile.hpp"
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Acheron::Core::Profile;

class TestProfile : public QObject
{
    Q_OBJECT
private slots:
    void validNames()
    {
        QCOMPARE(fromArguments({}), QStringLiteral("principal"));
        QCOMPARE(fromArguments({"--profile", "A_1-B"}), QStringLiteral("a_1-b"));
        QCOMPARE(fromArguments({"--profile=SECONDARY"}), QStringLiteral("secondary"));
        QCOMPARE(canonicalName(QString(32, 'Z')), QString(32, 'z'));
    }

    void discoverSavedNames()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QDir root(temp.path());
        for (const auto &name : {"Acheron-Custom-principal", "Acheron-Custom-2_demo",
                                 "Acheron", "Acheron-Custom-", "Acheron-Custom-bad.name"})
            QVERIFY(root.mkdir(name));
        QFile file(root.filePath("Acheron-Custom-file"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(savedNames(root), (QStringList{"2_demo", "principal"}));
    }

    void credentialNames()
    {
        for (const QString &name : QStringList{"Acheron-Custom-principal", "Acheron-Custom-two"}) {
            QCoreApplication::setApplicationName(name);
#ifdef Q_OS_WIN
            QCOMPARE(tokenKey("42"), name + QStringLiteral(":account_42_token"));
#else
            QCOMPARE(tokenKey("42"), QStringLiteral("account_42_token"));
#endif
            QVERIFY(tokenKey("42") != tokenKey("43"));
        }
    }

    void invalidNames()
    {
        for (const auto &name : QStringList{"", "-a", "_a", "A B", "../a", "a/b", "a.b", "a\n",
                                            " a ", QString(QChar(0x212a)), QString(33, 'a')})
            QVERIFY2(canonicalName(name).isEmpty(), qPrintable(name));
        QVERIFY(fromArguments({"--profile"}).isEmpty());
        QVERIFY(fromArguments({"--profile="}).isEmpty());
        QVERIFY(fromArguments({"--profile", "one", "--profile=two"}).isEmpty());
        QVERIFY(fromArguments({"--profile=a", "--profile=a"}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestProfile)
#include "tst_Profile.moc"
