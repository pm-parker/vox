#include "store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

Store::Store(const QString &directory, QObject *parent)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/vox"
                                      : directory) {
    QDir().mkpath(m_directory);
    load();
}

void Store::load() {
    QJsonDocument doc;
    if (readJson(m_directory + "/settings.json", &doc))
        m_settings = doc.object().toVariantMap();

    if (readJson(m_directory + "/dictionary.json", &doc))
        m_dictionary = doc.object().value("words").toArray().toVariantList();

    if (readJson(m_directory + "/history.json", &doc))
        m_history = doc.object().value("items").toArray().toVariantList();
}

bool Store::readJson(const QString &path, QJsonDocument *out) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const auto parsed = QJsonDocument::fromJson(file.readAll());
    if (parsed.isNull())
        return false;
    *out = parsed;
    return true;
}

// Rename-based atomic write: the temp file sits beside the target on the same
// filesystem, so the rename cannot tear. Nothing observes a half-written file.
bool Store::writeAtomically(const QString &path, const QByteArray &bytes) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (file.write(bytes) != bytes.size())
        return false;
    return file.commit();
}

void Store::save(const QString &leaf, const QJsonObject &object) {
    writeAtomically(m_directory + "/" + leaf, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QVariant Store::setting(const QString &key, const QVariant &fallback) const {
    return m_settings.value(key, fallback);
}

void Store::setSetting(const QString &key, const QVariant &value) {
    if (m_settings.value(key) == value)
        return;
    m_settings.insert(key, value);
    QVariantMap object = m_settings;
    save("settings.json", QJsonObject::fromVariantMap(object));
    emit settingsChanged();
}

QString Store::punctStyle() const { return m_settings.value("punctStyle", "cn").toString(); }

void Store::setPunctStyle(const QString &style) { setSetting("punctStyle", style); }

bool Store::autoInsert() const { return m_settings.value("autoInsert", true).toBool(); }

void Store::setAutoInsert(bool on) { setSetting("autoInsert", on); }

bool Store::autoStop() const { return m_settings.value("autoStop", true).toBool(); }

void Store::setAutoStop(bool on) { setSetting("autoStop", on); }

// crostivoice shipped these exact timings after browser field work: 1.6 s of
// silence ends a take, 9 s of nothing before the first word gives up, 120 s
// caps a wedged microphone. They felt right to users; they carry over.
int Store::gapMs() const { return m_settings.value("gapMs", 1600).toInt(); }
int Store::waitFirstMs() const { return m_settings.value("waitFirstMs", 9000).toInt(); }
int Store::maxMs() const { return m_settings.value("maxMs", 120000).toInt(); }

QString Store::insertMethod() const { return m_settings.value("insertMethod", "auto").toString(); }

void Store::setInsertMethod(const QString &method) { setSetting("insertMethod", method); }

QString Store::language() const { return m_settings.value("language", "auto").toString(); }

void Store::setLanguage(const QString &language) { setSetting("language", language); }

bool Store::useItn() const { return m_settings.value("useItn", true).toBool(); }

void Store::setUseItn(bool on) { setSetting("useItn", on); }

void Store::setGapMs(int ms) { setSetting("gapMs", qBound(400, ms, 5000)); }

QString Store::microphone() const { return m_settings.value("microphone", "").toString(); }

void Store::setMicrophone(const QString &source) { setSetting("microphone", source); }

QVariantList Store::dictionary() const { return m_dictionary; }

void Store::setDictionary(const QVariantList &pairs) {
    if (m_dictionary == pairs)
        return;
    m_dictionary = pairs;
    QJsonObject object;
    object.insert("words", QJsonArray::fromVariantList(m_dictionary));
    save("dictionary.json", object);
    emit dictionaryChanged();
}

void Store::addWord(const QString &from, const QString &to) {
    QVariantList pairs = m_dictionary;
    for (const auto &pair : pairs)
        if (pair.toMap().value("from") == from) {
            QVariantMap mutablePair = pair.toMap();
            mutablePair.insert("to", to);
            pairs.removeOne(pair);
            pairs.prepend(mutablePair);
            setDictionary(pairs);
            return;
        }
    QVariantMap pair;
    pair.insert("from", from);
    pair.insert("to", to);
    pairs.prepend(pair);
    setDictionary(pairs);
}

void Store::removeWord(const QString &from) {
    QVariantList pairs;
    for (const auto &pair : m_dictionary)
        if (pair.toMap().value("from") != from)
            pairs.append(pair);
    setDictionary(pairs);
}

// crostivoice learned the hard way to apply hot words as literal strings,
// longest first: a naive regex pass let short rules cannibalize longer ones
// ("four" inside "gpt four" became "gpt 4").
QString Store::applyDictionary(const QString &text) const {
    if (m_dictionary.isEmpty() || text.isEmpty())
        return text;
    QList<QPair<QString, QString>> pairs;
    pairs.reserve(m_dictionary.size());
    for (const auto &entry : m_dictionary) {
        const QString from = entry.toMap().value("from").toString();
        const QString to = entry.toMap().value("to").toString();
        if (!from.isEmpty())
            pairs.append({from, to});
    }
    std::sort(pairs.begin(), pairs.end(), [](const auto &a, const auto &b) {
        return a.first.size() > b.first.size();
    });
    QString result = text;
    for (const auto &pair : pairs)
        result.replace(pair.first, pair.second, Qt::CaseSensitive);
    return result;
}

QVariantList Store::history() const { return m_history; }

void Store::addHistory(const QString &text) {
    QVariantMap item;
    item.insert("text", text);
    item.insert("at", QDateTime::currentDateTime().toSecsSinceEpoch());
    QVariantList items;
    items.append(item);
    items.append(m_history);
    while (items.size() > 50)
        items.removeLast();
    m_history = items;
    QJsonObject object;
    object.insert("items", QJsonArray::fromVariantList(m_history));
    save("history.json", object);
    emit historyChanged();
}

void Store::clearHistory() {
    if (m_history.isEmpty())
        return;
    m_history.clear();
    QJsonObject object;
    object.insert("items", QJsonArray());
    save("history.json", object);
    emit historyChanged();
}
