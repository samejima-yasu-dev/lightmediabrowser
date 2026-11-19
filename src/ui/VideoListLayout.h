#pragma once

#include <QAbstractItemView>
#include <QListWidget>
#include <QScrollBar>
#include <QSize>

inline constexpr int VideoListVerticalPadding = 18;
inline constexpr int VideoListScrollStep = 6;

inline QSize videoListGridSize(const QSize &iconSize, int textLineHeight)
{
    const int contentHeight = iconSize.height() + 2 * textLineHeight + 32;
    return QSize(230, qMax(190, contentHeight));
}

inline int videoListMinimumHeight(const QSize &gridSize)
{
    return gridSize.height() + 2 * VideoListVerticalPadding;
}

inline void configureVideoListScrolling(QListWidget &videoList)
{
    videoList.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    videoList.verticalScrollBar()->setSingleStep(VideoListScrollStep);
}
