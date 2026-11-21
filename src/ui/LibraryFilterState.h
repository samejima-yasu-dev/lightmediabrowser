#pragma once

#include <QString>

struct LibraryFilterState
{
    QString query;
    qint64 tagId = 0;
    bool favoritesOnly = false;
    bool duplicatesOnly = false;
    int minPlayCount = 0;
    QString sortKey;

    QString normalizedSortKey() const
    {
        const QString key = sortKey.trimmed();
        if (key == QStringLiteral("newest") || key == QStringLiteral("name") ||
            key == QStringLiteral("size") || key == QStringLiteral("plays")) {
            return key;
        }
        return QStringLiteral("name");
    }
};
