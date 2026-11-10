#pragma once

#include <QJsonArray>
#include <QObject>
#include <QTimer>

// The daemon's public face for the Omarchy bar panel: a tiny JSON snapshot
// written atomically to ~/.local/state/vox/status.json on every state change.
// The panel polls it like Superwhisper's bar item polls its tray frames —
// no IPC, no socket lifetime questions, one well-known file. Level writes
// are throttled so a loud room doesn't spin the disk; state changes flush
// at once.
class StatusReporter : public QObject {
    Q_OBJECT

public:
    explicit StatusReporter(QObject *parent = nullptr);

    void setState(const QString &state);   // idle | listening | processing
    void setLevel(float level);            // 0..1
    void setLast(const QString &text, qint64 at);
    void setError(const QString &error);
    void setOffline();                     // panel shows "not running"

private:
    void flush();

    QString m_path;
    QString m_state = "idle";
    float m_level = 0;
    QString m_lastText;
    qint64 m_lastAt = 0;
    QString m_error;
    QTimer m_levelThrottle;
};
