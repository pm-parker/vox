#pragma once

#include <QFileSystemWatcher>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    // Brand yellow, kept separate from the theme accent so the app still
    // follows Omarchy's live theme underneath. QML picks brand while
    // duckYellow is on, and falls back to the theme accent when it is off.
    Q_PROPERTY(QString brand READ brand NOTIFY changed)
    Q_PROPERTY(QString brandForeground READ brandForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    // Signature mode: the duck-yellow brand colour is used for chrome.
    // Off = follow the live Omarchy theme like every other app.
    Q_PROPERTY(bool duckYellow READ duckYellow WRITE setDuckYellow NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QString brand() const { return QStringLiteral("#FFD338"); }
    QString brandForeground() const;
    int radius() const { return m_radius; }
    bool duckYellow() const { return m_duckYellow; }
    void setDuckYellow(bool on);

    static QString readAccent(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();
    static QString hyprSocketPath();
    void reconnectEvents();
    static QString foregroundFor(const QString &background);

    QString m_directory;
    QString m_accent = "#FFD60A";
    bool m_duckYellow = true;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingTimeout;
    // Hyprland socket2 event stream: configreloaded lines trigger one
    // rounding re-read (replaces the old 1 Hz hyprctl poll).
    QLocalSocket m_events;
};
