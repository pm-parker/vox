#include "engine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QThread>

namespace {
// The catalog: every model duckvox can download, with the exact flags its
// sherpa-onnx-offline invocation takes. URLs and sizes verified by HEAD
// request + probe extraction on 2026-11-10; all three archives unpack to
// <dirName>/ with the files listed.
QVector<ModelSpec> buildCatalog() {
    return {
        {"sense-voice", "SenseVoice",
         QObject::tr("zh/en/ja/ko/yue · punctuation + numbers · ~230 MB — recommended"),
         "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
         "sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17.tar.bz2",
         "sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17",
         "model.int8.onnx", "tokens.txt", 163'000'000, "sense-voice",
         {}},
        {"paraformer", "Paraformer",
         QObject::tr("zh + en · fastest decode, no punctuation · ~1 GB"),
         "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
         "sherpa-onnx-paraformer-zh-2024-03-09.tar.bz2",
         "sherpa-onnx-paraformer-zh-2024-03-09",
         "model.int8.onnx", "tokens.txt", 997'000'000, "paraformer",
         {}},
        {"zipformer", "Zipformer (multilingual)",
         QObject::tr("zh/en streaming-quality transducer · ~310 MB"),
         "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
         "sherpa-onnx-zipformer-zh-en-2023-11-22.tar.bz2",
         "sherpa-onnx-zipformer-zh-en-2023-11-22",
         // File names per the sherpa-onnx model docs: epoch-34-avg-19, int8
         // encoder/joiner, fp32 decoder (the archive ships no int8 decoder).
         "encoder-epoch-34-avg-19.int8.onnx", "tokens.txt", 312'000'000, "transducer",
         {}},
    };
}
} // namespace

const QVector<ModelSpec> &Engine::catalogSpecs() {
    static const QVector<ModelSpec> catalog = buildCatalog();
    return catalog;
}

QVariantList Engine::catalog() const {
    QVariantList list;
    for (const ModelSpec &entry : catalogSpecs()) {
        const bool installed = QFileInfo::exists(m_dir + "/" + entry.dirName + "/" + entry.modelFile);
        QVariantMap row;
        row.insert("id", entry.id);
        row.insert("name", entry.name);
        row.insert("detail", entry.detail);
        row.insert("installed", installed);
        list.append(row);
    }
    return list;
}

void Engine::setModel(const QString &model) {
    for (const ModelSpec &entry : catalogSpecs()) {
        if (entry.id == model && m_model != model) {
            m_model = model;
            refresh();
            // refresh() only signals when the phase flips; switching between
            // two installed models stays "ready", yet `model` (NOTIFY
            // phaseChanged) did change and the store must persist it.
            emit phaseChanged();
            return;
        }
    }
}

const ModelSpec &Engine::spec() const {
    for (const ModelSpec &entry : catalogSpecs())
        if (entry.id == m_model)
            return entry;
    return catalogSpecs().first();
}

Engine::Engine(const QString &modelsDirectory, QObject *parent)
    : QObject(parent),
      m_dir(modelsDirectory.isEmpty()
                ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/duckvox/models"
                : modelsDirectory) {
    QDir().mkpath(m_dir);
    m_bin = findBin(m_dir);

    // Download progress comes from the growing file on disk, not curl's
    // progress bar: robust against every buffering surprise.
    m_progressTimer.setInterval(400);
    connect(&m_progressTimer, &QTimer::timeout, this, &Engine::pollProgress);
    connect(&m_fetch, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                if (!m_downloading)
                    return;
                if (code != 0 || status != QProcess::NormalExit) {
                    m_progressTimer.stop();
                    m_downloading = false;
                    QFile::remove(m_dir + "/asr.tar.bz2");
                    m_error = tr("Model download failed. Check the network and try again.");
                    setPhase("error");
                    return;
                }
                finishDownload();
            });
    connect(&m_fetch, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!m_downloading)
            return;
        m_downloading = false;
        m_progressTimer.stop();
        QFile::remove(m_dir + "/asr.tar.bz2");
        m_error = tr("Model download failed: curl could not run. Check the network and try again.");
        setPhase("error");
    });

    // Extraction tail for the async unpacker: verify the expected files
    // landed, drop the archive either way, flip the phase.
    connect(&m_unpacker, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                if (m_phase != "extracting")
                    return; // stale signal (e.g. remove() killed the run)
                finishExtraction(code == 0 && status == QProcess::NormalExit);
            });
    connect(&m_unpacker, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_phase != "extracting")
            return;
        finishExtraction(false);
    });

    m_watchdog.setSingleShot(true);
    m_watchdog.setInterval(30000);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        if (m_worker.state() != QProcess::NotRunning)
            m_worker.kill();
        if (m_busy) {
            m_busy = false;
            emit busyChanged();
        }
        emit failed(tr("Recognition timed out. The model may be too heavy for this machine."));
    });
    connect(&m_worker, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                m_watchdog.stop();
                const bool wasBusy = m_busy;
                if (m_busy) {
                    m_busy = false;
                    emit busyChanged();
                }
                // Not busy means the watchdog already killed this run and
                // reported the timeout; a second "exited with an error"
                // would overwrite that clearer message.
                if (!wasBusy)
                    return;
                if (code != 0 || status != QProcess::NormalExit) {
                    emit failed(tr("The recognition engine exited with an error."));
                    return;
                }
                const QString text = parseOutput(m_worker.readAllStandardOutput() + m_worker.readAllStandardError());
                if (text.isEmpty())
                    emit failed(tr("Nothing recognizable in that recording — try again a bit closer to the mic."));
                else
                    emit transcribed(text);
            });

    refresh();
}

Engine::~Engine() {
    if (m_worker.state() != QProcess::NotRunning) {
        m_worker.kill();
        m_worker.waitForFinished(1000);
    }
    if (m_fetch.state() != QProcess::NotRunning) {
        m_fetch.kill();
        m_fetch.waitForFinished(1000);
    }
    if (m_unpacker.state() != QProcess::NotRunning) {
        m_unpacker.kill();
        m_unpacker.waitForFinished(1000);
    }
}

QString Engine::findBin(const QString &modelsDirectory) {
    // The package installs sherpa-onnx-offline into /usr/bin; a copy dropped
    // next to the models wins so a hand-placed build can be tested first.
    const QString bundled = modelsDirectory + "/sherpa-onnx-offline";
    if (QFileInfo(bundled).isExecutable())
        return bundled;
    const QString system = QStandardPaths::findExecutable("sherpa-onnx-offline");
    return QFileInfo(system).isExecutable() ? system : QString();
}

void Engine::setPhase(const QString &phase) {
    if (m_phase == phase)
        return;
    m_phase = phase;
    emit phaseChanged();
}

void Engine::refresh() {
    if (m_downloading || m_phase == "extracting")
        return;
    const ModelSpec &active = spec();
    const bool ready = QFileInfo::exists(m_dir + "/" + active.dirName + "/" + active.modelFile)
                       && !findBin(m_dir).isEmpty();
    if (ready || m_phase == "error")
        m_error.clear();
    setPhase(ready ? "ready" : "not-installed");
    m_bin = findBin(m_dir);
    if (ready)
        m_progress = 100;
}

void Engine::install() {
    if (m_downloading || m_phase == "ready" || m_phase == "extracting")
        return;
    const ModelSpec &active = spec();
    m_error.clear();
    m_progress = 0;
    setPhase("downloading");
    m_downloading = true;
    // --location follows the release redirect; --fail makes a 404 page exit
    // non-zero instead of being saved as a "model"; --continue-at resumes a
    // partial download after a network drop.
    m_fetch.start("curl", {"-fL", "--continue-at", "-", "--no-progress-meter",
                           "-o", m_dir + "/asr.tar.bz2", active.archiveUrl});
    if (!m_fetch.waitForStarted(3000)) {
        m_downloading = false;
        m_error = tr("Model download failed: install curl first.");
        setPhase("error");
        return;
    }
    m_progressTimer.start();
}

void Engine::pollProgress() {
    if (!m_downloading)
        return;
    const qint64 got = QFileInfo(m_dir + "/asr.tar.bz2").size();
    m_progress = int(qBound<qint64>(1, got * 100 / spec().bytes, qint64(99)));
    emit phaseChanged();
}

void Engine::finishDownload() {
    m_progressTimer.stop();
    m_downloading = false;
    const QString archive = m_dir + "/asr.tar.bz2";
    const qint64 got = QFileInfo(archive).size();
    if (got < 1000) {
        QFile::remove(archive);
        m_error = tr("Model download failed. Check the network and try again.");
        setPhase("error");
        return;
    }
    setPhase("extracting");
    // Unpack OFF the GUI thread: a synchronous waitForFinished here froze
    // hotkeys, the control socket and the window for as long as tar needed
    // (tens of seconds on these archives, against a 10 min timeout).
    // m_unpacker's finished/error connects (constructor) call
    // finishExtraction when it lands.
    m_unpacker.setWorkingDirectory(m_dir);
    m_unpacker.start("tar", {"xjf", archive});
    if (!m_unpacker.waitForStarted(3000))
        finishExtraction(false);
}

// Shared tail of the async extraction: verify the expected files landed,
// clean the archive either way, and flip the phase.
void Engine::finishExtraction(bool ok) {
    QFile::remove(m_dir + "/asr.tar.bz2");
    m_bin = findBin(m_dir);
    const ModelSpec &active = spec();
    if (ok && !m_bin.isEmpty()
        && QFileInfo::exists(m_dir + "/" + active.dirName + "/" + active.modelFile)) {
        m_progress = 100;
        setPhase("ready");
        return;
    }
    m_error = ok ? tr("The model archive did not contain what duckvox expected.")
                 : tr("The downloaded model archive could not be extracted.");
    setPhase("error");
}

void Engine::remove() {
    if (m_unpacker.state() != QProcess::NotRunning) {
        m_unpacker.kill();
        m_unpacker.waitForFinished(1000);
    }
    QDir(m_dir + "/" + spec().dirName).removeRecursively();
    QFile::remove(m_dir + "/asr.tar.bz2");
    m_progress = 0;
    refresh();
}

// sherpa-onnx 2.x prints one JSON object per file ("text" holds the
// transcript) and dumps its whole config to stderr; older builds print a
// plain "Recognized text:" line. Only those two forms are transcript —
// everything else on the stream is log chatter and must never reach the
// text pipeline, which would happily "punctuate" it.
QString Engine::parseOutput(const QByteArray &raw) {
    const QStringList lines = QString::fromUtf8(raw).split('\n', Qt::SkipEmptyParts);
    QStringList jsonTexts;
    QStringList prefixed;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.startsWith('{')) {
            const QJsonObject object = QJsonDocument::fromJson(line.toUtf8()).object();
            const QString text = object.value("text").toString().trimmed();
            if (!text.isEmpty())
                jsonTexts.append(text);
            continue;
        }
        for (const char *prefix : {"Recognized text:", "Text:"}) {
            const QString head = QString::fromLatin1(prefix);
            if (line.startsWith(head, Qt::CaseInsensitive)) {
                const QString text = line.mid(head.size()).trimmed();
                if (!text.isEmpty())
                    prefixed.append(text);
                break;
            }
        }
    }
    if (!jsonTexts.isEmpty())
        return jsonTexts.join(' ').trimmed();
    return prefixed.join(' ').trimmed();
}

void Engine::transcribe(const QString &wavPath, const QString &language, bool useItn) {
    if (m_phase != "ready" || m_bin.isEmpty()) {
        emit failed(tr("The speech model is not installed yet — press M in the window to download it once."));
        return;
    }
    if (m_busy)
        return;
    const ModelSpec &active = spec();
    const QString root = m_dir + "/" + active.dirName;
    // Flag names follow the 2.x CLI. SenseVoice natively punctuates and
    // inverse-normalizes; Paraformer and Zipformer transducers need the
    // shared encoder/decoder/joiner triple. Language and ITN flags are
    // SenseVoice-only — the other kinds ignore those settings.
    QStringList args;
    if (active.kind == "sense-voice") {
        args << "--sense-voice-model=" + root + "/" + active.modelFile;
    } else if (active.kind == "paraformer") {
        args << "--paraformer-model=" + root + "/" + active.modelFile;
    } else {
        // Offline transducers take the encoder/decoder/joiner triple; there is
        // no --zipformer2-model flag, and sherpa-onnx exits on unknown flags.
        args << "--encoder=" + root + "/" + active.modelFile
             << "--decoder=" + root + "/decoder-epoch-34-avg-19.onnx"
             << "--joiner=" + root + "/joiner-epoch-34-avg-19.int8.onnx";
    }
    args << "--tokens=" + root + "/" + active.tokensFile;
    if (active.kind == "sense-voice") {
        args << "--sense-voice-use-itn=" + (useItn ? QStringLiteral("true") : QStringLiteral("false"))
             << "--sense-voice-language=" + (language.isEmpty() ? QStringLiteral("auto") : language);
    }
    args << "--num-threads=" + QString::number(qMax(1, QThread::idealThreadCount() - 1))
         << wavPath;
    m_worker.start(m_bin, args);
    if (!m_worker.waitForStarted(5000)) {
        emit failed(tr("The recognition engine could not start."));
        return;
    }
    m_busy = true;
    emit busyChanged();
    // The watchdog must outlast a legitimately long take: a fixed 30 s cut
    // off anything past ~60 s of audio on a laptop CPU (RTF ~0.5) even though
    // the recorder allows 120 s. Budget 30 s plus the audio's own length.
    const qint64 audioMs = qMax<qint64>(0, QFileInfo(wavPath).size() - 44) / 32; // s16le mono 16 kHz
    m_watchdog.start(int(qMin<qint64>(30000 + audioMs, 10 * 60 * 1000)));
}
