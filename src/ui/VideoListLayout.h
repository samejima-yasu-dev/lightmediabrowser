#pragma once

#include <QSize>

inline constexpr int VideoListVerticalPadding = 18;

inline QSize videoListGridSize(const QSize &iconSize, int textLineHeight)
{
    const int contentHeight = iconSize.height() + 2 * textLineHeight + 32;
    return QSize(230, qMax(190, contentHeight));
}

inline int videoListMinimumHeight(const QSize &gridSize)
{
    return gridSize.height() + 2 * VideoListVerticalPadding;
}
