#pragma once

#include <QDateTime>
#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QVariantList>

// Settings, history, and the user dictionary, persisted as JSON under one
// root (default ~/.local/share/vox). Every write is atomic — a temp file
// beside the target, then rename — so a crash never leaves half a store, the
// same care Omacut takes with exports. History is capped like crostivoice's
// (50 items): a recall aid, not an archive.
class Store : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool autoInsert READ autoInsert WRITE setAutoInsert NOTIFY settingsChanged)
    Q_PROPERTY(bool autoStop READ autoStop WRITE setAutoStop NOTIFY settingsChanged)
    Q_PROPERTY(QString punctStyle READ punctStyle WRITE setPunctStyle NOTIFY settingsChanged)
    Q_PROPERTY(QString insertMethod READ insertMethod WRITE setInsertMethod NOTIFY settingsChanged)
    // SenseVoice's recognition language: auto|zh|en|ja|ko|yue (the model's set).
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY settingsChanged)
    // Inverse text normalization: "二零二五" -> "2025", raw digits -> numbered.
    Q_PROPERTY(bool useItn READ useItn WRITE setUseItn NOTIFY settingsChanged)
    // Silence that ends a tap-to-dictate take.
    Q_PROPERTY(int gapMs READ gapMs WRITE setGapMs NOTIFY settingsChanged)
    // PulseAudio source name; "" follows the system default.
    Q_PROPERTY(QString microphone READ microphone WRITE setMicrophone NOTIFY settingsChanged)
    // Active speech model id (see Engine::catalog); Engine downloads and
    // transcribes with it.
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY settingsChanged)
    // Launch vox --minimized at login (Omarchy autostart entry).
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY settingsChanged)
    // Global hotkeys as {toggleKey, pttKey}, applied through
    // ~/.config/vox/shortcuts.lua by omarchy-dictation-vox-keys.
    Q_PROPERTY(QJsonObject shortcuts READ shortcuts WRITE setShortcuts NOTIFY settingsChanged)

public:
    explicit Store(const QString &directory = {}, QObject *parent = nullptr);

    // --- settings ---
    QVariant setting(const QString &key, const QVariant &fallback = {}) const;
    void setSetting(const QString &key, const QVariant &value);

    // Punctuation style is a first-class setting because crostivoice users
    // flip it per task (code comments want half-width, prose wants full).
    QString punctStyle() const;               // "cn" | "en"
    void setPunctStyle(const QString &style); //
    bool autoInsert() const;
    void setAutoInsert(bool on);
    bool autoStop() const;
    void setAutoStop(bool on);
    int gapMs() const;                        // silence that ends a take
    int waitFirstMs() const;                  // grace before the first word
    int maxMs() const;                        // absolute ceiling on a take
    QString insertMethod() const;             // "auto" | "wtype" | "clipboard"
    void setInsertMethod(const QString &method);
    QString language() const;
    void setLanguage(const QString &language);
    bool useItn() const;
    void setUseItn(bool on);
    void setGapMs(int ms);
    QString microphone() const;
    void setMicrophone(const QString &source);
    QString model() const;
    void setModel(const QString &model);
    bool autostart() const;
    void setAutostart(bool on);
    QJsonObject shortcuts() const;
    void setShortcuts(const QJsonObject &keys);

    // --- dictionary ---
    QVariantList dictionary() const;
    void setDictionary(const QVariantList &pairs);
    void addWord(const QString &from, const QString &to);
    void removeWord(const QString &from);
    // Longest-first literal replacement: "gpt four -> GPT-4" must win over a
    // shorter "four -> 4" that would otherwise corrupt it.
    QString applyDictionary(const QString &text) const;

    // --- history ---
    QVariantList history() const;
    void addHistory(const QString &text);
    void clearHistory();

    static bool writeAtomically(const QString &path, const QByteArray &bytes);
    static bool readJson(const QString &path, class QJsonDocument *out);

signals:
    void dictionaryChanged();
    void historyChanged();
    void settingsChanged();

private:
    void load();
    void save(const QString &leaf, const class QJsonObject &object);

    QString m_directory;
    QVariantMap m_settings;
    QVariantList m_dictionary;
    QVariantList m_history;
};
