#pragma once

#include <QObject>
#include <QVariantList>

class Engine;
class Inserter;
class Recorder;
class Store;

// The session state machine tying capture, recognition, polishing, and
// insertion together: idle -> listening -> processing -> idle. QML renders
// this state and calls the invokables; everything that can fail happens in
// the collaborators, and their failures land in errorText — visible in the
// window, never silently swallowed.
class Backend : public QObject {
    Q_OBJECT
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

public:
    explicit Backend(Store *store, Engine *engine, Recorder *recorder, Inserter *inserter,
                     QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString lastText() const { return m_lastText; }
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
    Q_INVOKABLE void useHistoryText(int index);
    Q_INVOKABLE void dismissError();
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void addWord(const QString &from, const QString &to);
    Q_INVOKABLE void removeWord(const QString &from);
    // Pulse sources for the microphone picker, refreshed on demand.
    Q_INVOKABLE void refreshMics();

    QString language() const;
    void setLanguage(const QString &language);
    bool useItn() const;
    void setUseItn(bool on);
    int gapMs() const;
    void setGapMs(int ms);
    QString microphone() const;
    void setMicrophone(const QString &source);
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

    Store *m_store;
    Engine *m_engine;
    Recorder *m_recorder;
    Inserter *m_inserter;
    QString m_state = "idle";
    QString m_lastText;
    QString m_statusLine;
    QString m_errorText;
    QVariantList m_micSources;
};
