#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    // Brand mode: the duck-yellow signature stays on regardless of the
    // Omarchy theme. Off = follow the live theme like every other app.
    Q_PROPERTY(bool duckYellow READ duckYellow WRITE setDuckYellow NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_duckYellow ? QStringLiteral("#FFD338") : m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }
    bool duckYellow() const { return m_duckYellow; }
    void setDuckYellow(bool on);

    static QString readAccent(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = "#FFD60A";
    bool m_duckYellow = true;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
