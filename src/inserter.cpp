#include "inserter.h"

#include <QStandardPaths>

Inserter::Inserter(QObject *parent) : QObject(parent) {
    connect(&m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &m_proc,
            &QProcess::deleteLater);
}

bool Inserter::runWtype(const QString &text) {
    const QString wtype = QStandardPaths::findExecutable("wtype");
    if (wtype.isEmpty())
        return false;
    QProcess proc;
    // -s gives slow apps time to focus. "--" ends wtype's option parsing, so a
    // transcript starting with '-' ("-5 度", "- 第一项") is typed, not rejected
    // as an unknown flag.
    proc.start(wtype, {"-s", "1000", "--", text});
    if (!proc.waitForFinished(5000) || proc.exitCode() != 0) {
        proc.kill();
        return false;
    }
    return true;
}

bool Inserter::runCopy(const QString &text) {
    const QString wlcopy = QStandardPaths::findExecutable("wl-copy");
    if (wlcopy.isEmpty())
        return false;
    QProcess proc;
    proc.start(wlcopy, {});
    proc.write(text.toUtf8());
    proc.closeWriteChannel();
    if (!proc.waitForFinished(3000) || proc.exitCode() != 0) {
        proc.kill();
        return false;
    }
    return true;
}

bool Inserter::insert(const QString &text, const QString &method) {
    if (text.isEmpty())
        return false;
    bool ok = false;
    QString used;
    if (method == "wtype") {
        ok = runWtype(text);
        used = ok ? "wtype" : QString();
    } else if (method == "clipboard") {
        ok = runCopy(text);
        used = ok ? "clipboard" : QString();
    } else {
        if (runWtype(text)) {
            ok = true;
            used = "wtype";
        } else if (runCopy(text)) {
            ok = true;
            used = "clipboard";
        }
    }
    if (!ok)
        return false;
    m_lastMethod = used;
    emit inserted(used);
    return true;
}
