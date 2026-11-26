#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

bool runDatabaseBenchmark(const QString &videoDirectory, const QList<int> &counts,
                          QJsonObject *report, QString *error = nullptr);
