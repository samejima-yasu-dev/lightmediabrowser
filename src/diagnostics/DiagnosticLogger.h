#pragma once

#include <QString>

bool installDiagnosticLogger(QString *logPath = nullptr, QString *error = nullptr);
QString diagnosticLogPath();
