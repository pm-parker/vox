#pragma once

#include <QObject>
#include <QJsonObject>
#include <QVariantList>

class Engine;
class Inserter;
class Recorder;
class Store;

// The build injects the release version (vox.pro VERSION → APP_VERSION).
// Builds outside qmake — the test harness — fall back to a marker instead
// of failing to compile.
#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

// The session state machine tying capture, recognition, polishing, and
// insertion together: idle -> listening -> processing -> idle. QML renders
// this state and calls the invokables; everything that can fail happens in
// the collaborators, and their failures land in errorText — visible in the
// window, never silently swallowed.
class Backend : public QObject {
    Q_OBJECT
    // The running release version (from the build system's APP_VERSION),
    // shown in the About card and by `duckvox --version`.
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged) // idle|listening|processing
    Q_PROPERTY(QString lastText READ lastText NOTIFY lastTextChanged)
    Q_PROPERTY(float level READ level NOTIFY levelChanged)
    Q_PROPERTY(int elapsedMs READ elapsedMs NOTIFY elapsedChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString statusLine READ statusLine NOTIFY statusChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    // Same settings as Store, mirrored so QML reads one type (Settings and
    // Backend both work); writes land in the store and fire settingsChanged.
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY settingsChanged)
    Q_PROPERTY(bool useItn READ useItn WRITE setUseItn NOTIFY settingsChanged)
    Q_PROPERTY(int gapMs READ gapMs WRITE setGapMs NOTIFY settingsChanged)
    Q_PROPERTY(QString microphone READ microphone WRITE setMicrophone NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList dictionary READ dictionary NOTIFY dictionaryChanged)
    Q_PROPERTY(QVariantList micSources READ micSources NOTIFY micSourcesChanged)
    // The active speech model (Engine downloads/transcribes with it) and
    // whether duckvox starts minimized at login.
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY settingsChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY settingsChanged)
    // Global hotkeys {toggleKey, pttKey} — Hyprland syntax. Writes go to
    // the store and immediately re-run omarchy-dictation-duckvox-keys, which
    // regenerates ~/.config/duckvox/shortcuts.lua and reloads the binds.
    Q_PROPERTY(QJsonObject shortcuts READ shortcuts WRITE setShortcuts NOTIFY settingsChanged)

public:
    explicit Backend(Store *store, Engine *engine, Recorder *recorder, Inserter *inserter,
                     QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString lastText() const { return m_lastText; }
    QString version() const;
    float level() const;
    int elapsedMs() const;
    QVariantList history() const;
    QString statusLine() const { return m_statusLine; }
    QString errorText() const { return m_errorText; }

    Q_INVOKABLE void toggle();
    // Push-to-talk: hold the key to talk, release to insert. The silence gap
    // that ends a tap-to-dictate take must not cut a held one short, so the
    // hold ignores it and ends on the release instead.
    Q_INVOKABLE void holdStart();
    Q_INVOKABLE void holdStop();
    Q_INVOKABLE void repeatLast();
    Q_INVOKABLE void copyLast();
    // The per-item delta for a take (what it appended to the previous
    // history text); falls back to the full text when unknown. Backs the
    // C shortcut and the per-row "copy new part" button.
    Q_INVOKABLE QString deltaOf(const QString &text) const;
    // Copy arbitrary text to the clipboard (per-row "copy new part").
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE void useHistoryText(int index);
    Q_INVOKABLE void dismissError();
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void addWord(const QString &from, const QString &to);
    Q_INVOKABLE void removeWord(const QString &from);
    // Pulse sources for the microphone picker, refreshed on demand.
    Q_INVOKABLE void refreshMics();
    // Apply the autostart setting to the Omarchy startup list. Called once
    // by main() after construction (deferred, so the autostart file exists
    // even on a cold first boot) and again whenever the setting changes.
    // ON is the default, on purpose: reaching this app means the user ran
    // `omarchy install dictation duckvox`, and Omarchy dictation backends
    // are expected to come back at login. The toggle turns it off (the
    // launch line is swapped for a guard comment, so re-enabling is one
    // flip); nothing is ever added while the setting is off.
    Q_INVOKABLE void applyAutostart();
    // Regenerate the shortcut file and reload the binds (runs the packaged
    // omarchy-dictation-duckvox-keys). resetShortcuts also clears the saved
    // overrides, returning the card to the Alt+Space / Alt_R defaults.
    Q_INVOKABLE void applyShortcuts();
    Q_INVOKABLE void resetShortcuts();

    QString language() const;
    void setLanguage(const QString &language);
    bool useItn() const;
    void setUseItn(bool on);
    int gapMs() const;
    void setGapMs(int ms);
    QString microphone() const;
    void setMicrophone(const QString &source);
    QString model() const;
    void setModel(const QString &model);
    bool autostart() const;
    void setAutostart(bool on);
    QJsonObject shortcuts() const;
    void setShortcuts(const QJsonObject &keys);
    QVariantList dictionary() const;
    QVariantList micSources() const;

signals:
    void stateChanged();
    void lastTextChanged();
    void levelChanged();
    void elapsedChanged();
    void historyChanged();
    void statusChanged();
    void errorChanged();
    void dictionaryChanged();
    void micSourcesChanged();
    // Forwarded from Store::settingsChanged so the mirrored settings
    // properties (language/useItn/gapMs/microphone) have a NOTIFY of their own.
    void settingsChanged();

private:
    void onTakeFinished(const QString &wavPath, const QString &reason);
    void onRecognized(const QString &raw);
    void insertAndReport(const QString &text);
    void discardTake();

    Store *m_store;
    Engine *m_engine;
    Recorder *m_recorder;
    Inserter *m_inserter;
    QString m_state = "idle";
    QString m_pendingTake; // WAV handed to the engine, deleted once it answers
    QString m_lastText;
    QString m_statusLine;
    QString m_errorText;
    QVariantList m_micSources;
};
