#include "statusreporter.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "store.h"

StatusReporter::StatusReporter(QObject *parent) : QObject(parent) {
    const QString stateDir = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    m_path = stateDir + "/vox/status.json";
    QDir().mkpath(stateDir + "/vox");
    m_levelThrottle.setInterval(200);
    m_levelThrottle.setSingleShot(true);
    connect(&m_levelThrottle, &QTimer::timeout, this, &StatusReporter::flush);
}

void StatusReporter::setState(const QString &state) {
    if (m_state == state)
        return;
    m_state = state;
    if (state == "idle")
        m_level = 0;
    flush();
}

void StatusReporter::setLevel(float level) {
    if (m_state != "listening")
        return;
    m_level = level;
    if (!m_levelThrottle.isActive())
        m_levelThrottle.start();
}

void StatusReporter::setLast(const QString &text, qint64 at) {
    m_lastText = text;
    m_lastAt = at;
    flush();
}

void StatusReporter::setError(const QString &error) {
    if (m_error == error)
        return;
    m_error = error;
    flush();
}

void StatusReporter::setOffline() {
    QFile::remove(m_path);
}

void StatusReporter::flush() {
    QJsonObject object;
    object.insert("version", 1);
    object.insert("state", m_state);
    object.insert("level", m_level);
    if (!m_lastText.isEmpty()) {
        object.insert("lastText", m_lastText);
        object.insert("lastAt", m_lastAt);
    }
    if (!m_error.isEmpty())
        object.insert("error", m_error);
    Store::writeAtomically(m_path, QJsonDocument(object).toJson(QJsonDocument::Compact));
}
