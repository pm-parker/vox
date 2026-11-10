#pragma once

#include <QDateTime>
#include <QObject>
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
    Q_PROPERTY(int gapMs READ gapMs CONSTANT)

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
