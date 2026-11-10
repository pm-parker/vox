#pragma once

#include <QObject>
#include <QProcess>

// Text lands where the user is already typing, by preference: wtype types
// into the focused Wayland window directly; wl-copy keeps the text on the
// clipboard as the fallback and says so. Only the GUI terminal window itself
// receives paste-in-window. Tests inject a fake wtype through PATH.
class Inserter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastMethod READ lastMethod NOTIFY inserted)

public:
    explicit Inserter(QObject *parent = nullptr);

    QString lastMethod() const { return m_lastMethod; }

    // method: "auto" tries wtype then falls back to the clipboard; the other
    // values force one path so tests and power users can pin behavior.
    bool insert(const QString &text, const QString &method);

signals:
    void inserted(const QString &method);

private:
    bool runWtype(const QString &text);
    bool runCopy(const QString &text);

    QString m_lastMethod;
    QProcess m_proc;
};
