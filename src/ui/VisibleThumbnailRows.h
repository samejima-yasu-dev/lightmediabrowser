#pragma once

#include <QList>
#include <QRect>

inline QList<int> visibleThumbnailRows(const QList<QRect> &itemRects, const QRect &viewport)
{
    QList<int> rows;
    for (int index = 0; index < itemRects.size(); ++index) {
        if (itemRects.at(index).intersects(viewport)) rows.append(index);
    }
    return rows;
}
