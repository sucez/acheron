#include "Core/TokenStore.hpp"

#include <QTest>
#include <QUuid>

using Acheron::Core::Snowflake;
using Acheron::Core::TokenStore;

class TestWindowsTokenStore : public QObject
{
    Q_OBJECT

    // Zero cannot identify a Discord account, even if profile scoping regresses.
    const Snowflake account{0};
    const QString prefix = QStringLiteral("Acheron-Custom-test-")
            + QUuid::createUuid().toString(QUuid::Id128).left(24);
    const QString first = prefix + QStringLiteral("-a");
    const QString second = prefix + QStringLiteral("-b");

    bool hasValue(const QString &profile, const QString &expected) const
    {
        QCoreApplication::setApplicationName(profile);
        const auto result = TokenStore::loadToken(account);
        return result.value == expected;
    }

private slots:
    void sameAccountInTwoProfiles()
    {
        QCoreApplication::setApplicationName(first);
        const auto savedFirst = TokenStore::saveToken(account, "fixture-first");
        QVERIFY2(savedFirst.success(), qPrintable(savedFirst.error));

        QCoreApplication::setApplicationName(second);
        const auto savedSecond = TokenStore::saveToken(account, "fixture-second");
        QVERIFY2(savedSecond.success(), qPrintable(savedSecond.error));

        QVERIFY(hasValue(first, "fixture-first"));
        QVERIFY(hasValue(second, "fixture-second"));

        QCoreApplication::setApplicationName(first);
        const auto replaced = TokenStore::saveToken(account, "fixture-replaced");
        QVERIFY2(replaced.success(), qPrintable(replaced.error));
        QVERIFY(hasValue(first, "fixture-replaced"));
        QVERIFY(hasValue(second, "fixture-second"));

        QCoreApplication::setApplicationName(first);
        QVERIFY(TokenStore::deleteToken(account));
        QVERIFY(!TokenStore::loadToken(account).success());
        QVERIFY(hasValue(second, "fixture-second"));
    }

    void cleanup()
    {
        // Qt Test also runs cleanup after a failed assertion in the test slot.
        QCoreApplication::setApplicationName(first);
        const bool removedFirst = TokenStore::deleteToken(account);
        QCoreApplication::setApplicationName(second);
        const bool removedSecond = TokenStore::deleteToken(account);
        QVERIFY(removedFirst);
        QVERIFY(removedSecond);
    }
};

QTEST_GUILESS_MAIN(TestWindowsTokenStore)
#include "tst_WindowsTokenStore.moc"
