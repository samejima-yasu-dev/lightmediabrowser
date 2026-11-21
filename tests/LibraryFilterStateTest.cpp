#include "ui/LibraryFilterState.h"

#include <QCoreApplication>
#include <QtTest>

void defaultsAndNormalizedSortKey()
{
    LibraryFilterState state;
    QCOMPARE(state.tagId, qint64(0));
    QVERIFY(!state.favoritesOnly);
    QVERIFY(!state.duplicatesOnly);
    QCOMPARE(state.minPlayCount, 0);
    QCOMPARE(state.normalizedSortKey(), QStringLiteral("name"));

    state.sortKey = QStringLiteral(" newest ");
    QCOMPARE(state.normalizedSortKey(), QStringLiteral("newest"));

    state.sortKey = QStringLiteral("unknown");
    QCOMPARE(state.normalizedSortKey(), QStringLiteral("name"));
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    defaultsAndNormalizedSortKey();
    return 0;
}
