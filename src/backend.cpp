#include "backend.h"

#include "engine.h"
#include "inserter.h"
#include "recorder.h"
#include "store.h"
#include "textpipeline.h"

Backend::Backend(Store *store, Engine *engine, Recorder *recorder, Inserter *inserter, QObject *parent)
    : QObject(parent), m_store(store), m_engine(engine), m_recorder(recorder), m_inserter(inserter) {
    connect(m_recorder, &Recorder::stopped, this, &Backend::onTakeFinished);
    connect(m_recorder, &Recorder::failed, this, [this](const QString &message) {
        if (m_state == "listening")
            m_state = "idle";
        m_errorText = message;
        emit stateChanged();
        emit errorChanged();
    });
    connect(m_recorder, &Recorder::levelChanged, this, &Backend::levelChanged);
    connect(m_recorder, &Recorder::tick, this, &Backend::elapsedChanged);
    connect(m_engine, &Engine::transcribed, this, &Backend::onRecognized);
    connect(m_engine, &Engine::failed, this, [this](const QString &message) {
        if (m_state == "processing")
            m_state = "idle";
        m_errorText = message;
        emit stateChanged();
        emit errorChanged();
    });
    connect(m_store, &Store::historyChanged, this, &Backend::historyChanged);
}

float Backend::level() const { return m_recorder->level(); }
int Backend::elapsedMs() const { return m_recorder->elapsedMs(); }
QVariantList Backend::history() const { return m_store->history(); }

void Backend::toggle() {
    if (m_state == "listening") {
        m_statusLine = tr("Finishing…");
        emit statusChanged();
        m_recorder->stop("manual");
        return;
    }
    if (m_state != "idle")
        return;
    m_errorText.clear();
    emit errorChanged();
    const bool started = m_recorder->start(m_store->gapMs(), m_store->waitFirstMs(), m_store->maxMs(),
                                           m_store->autoStop(), 0.012f);
    if (!started)
        return; // recorder already reported the failure
    m_state = "listening";
    m_statusLine = tr("Listening — Space stops");
    emit stateChanged();
    emit statusChanged();
}

void Backend::holdStart() {
    if (m_state != "idle")
        return;
    m_errorText.clear();
    emit errorChanged();
    const bool started = m_recorder->start(m_store->gapMs(), m_store->waitFirstMs(), m_store->maxMs(),
                                           false, 0.012f);
    if (!started)
        return; // recorder already reported the failure
    m_state = "listening";
    m_statusLine = tr("Listening — release to insert");
    emit stateChanged();
    emit statusChanged();
}

void Backend::holdStop() {
    if (m_state != "listening")
        return;
    m_statusLine = tr("Finishing…");
    emit statusChanged();
    m_recorder->stop("hold-release");
}

void Backend::onTakeFinished(const QString &wavPath, const QString &reason) {
    if (m_state != "listening")
        return;
    if (reason == "silence") {
        m_state = "idle";
        m_statusLine.clear();
        m_errorText = tr("Didn't catch any speech — check the microphone and try again.");
        emit stateChanged();
        emit statusChanged();
        emit errorChanged();
        return;
    }
    m_state = "processing";
    emit stateChanged();
    m_engine->transcribe(wavPath, "auto");
}

void Backend::onRecognized(const QString &raw) {
    if (m_state == "processing")
        m_state = "idle";
    const QString text = TextPipeline::process(raw, *m_store).trimmed();
    if (text.isEmpty()) {
        m_errorText = tr("Nothing recognizable in that recording — try again a bit closer to the mic.");
        emit errorChanged();
        emit stateChanged();
        return;
    }
    m_lastText = text;
    emit lastTextChanged();
    m_store->addHistory(text);
    emit stateChanged();
    insertAndReport(text);
}

void Backend::insertAndReport(const QString &text) {
    if (m_store->autoInsert()) {
        if (m_inserter->insert(text, m_store->insertMethod()))
            m_statusLine = tr("Inserted into the focused window (%1)").arg(m_inserter->lastMethod());
        else {
            m_statusLine.clear();
            m_errorText = tr("Could not insert: wtype and wl-copy both failed. The text is kept here.");
            emit errorChanged();
        }
    } else {
        m_statusLine = tr("Ready — press R to insert, C to copy");
    }
    emit statusChanged();
}

void Backend::repeatLast() {
    if (m_lastText.isEmpty() || m_state != "idle")
        return;
    insertAndReport(m_lastText);
}

void Backend::copyLast() {
    if (m_lastText.isEmpty())
        return;
    if (m_inserter->insert(m_lastText, "clipboard"))
        m_statusLine = tr("Copied to the clipboard");
    else {
        m_statusLine.clear();
        m_errorText = tr("Could not insert: wtype and wl-copy both failed. The text is kept here.");
        emit errorChanged();
    }
    emit statusChanged();
}

void Backend::useHistoryText(int index) {
    const QVariantList items = m_store->history();
    if (index < 0 || index >= items.size())
        return;
    m_lastText = items.at(index).toMap().value("text").toString();
    m_statusLine = tr("Recalled from history");
    emit lastTextChanged();
    emit statusChanged();
}

void Backend::dismissError() {
    if (m_errorText.isEmpty())
        return;
    m_errorText.clear();
    emit errorChanged();
}
