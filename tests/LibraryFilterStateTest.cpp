#include "ui/LibraryFilterState.h"

#include <QCoreApplication>
#include <QtTest>

bool defaultsAndNormalizedSortKey()
{
    LibraryFilterState state;
    if (state.tagId != 0 || state.favoritesOnly || state.duplicatesOnly ||
        state.minPlayCount != 0 || state.normalizedSortKey() != QStringLiteral("name")) {
        return false;
    }

    state.sortKey = QStringLiteral(" newest ");
    if (state.normalizedSortKey() != QStringLiteral("newest")) return false;

    state.sortKey = QStringLiteral("unknown");
    return state.normalizedSortKey() == QStringLiteral("name");
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    return defaultsAndNormalizedSortKey() ? 0 : 1;
}
