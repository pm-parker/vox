#include "engine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>

namespace {
// HEAD-checked reachable from this machine. The same tarball carries the
// model, tokens, and the static sherpa-onnx-offline binary for this arch.
const char kArchiveUrl[] =
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
    "sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2";
const char kDirName[] = "sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17";
constexpr qint64 kArchiveBytes = 243'000'000; // approximate, for progress only
} // namespace

Engine::Engine(const QString &modelsDirectory, QObject *parent)
    : QObject(parent),
      m_dir(modelsDirectory.isEmpty()
                ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/vox/models"
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
                if (code != 0 || status != QProcess::NormalExit) {
                    emit failed(tr("The recognition engine exited with an error."));
                    return;
                }
                if (!wasBusy)
                    return;
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
}

QString Engine::findBin(const QString &modelsDirectory) {
    // The binary ships inside the model tarball (static, matches the arch it
    // was downloaded on); a system sherpa-onnx-offline is a fine substitute.
    const QString bundled = modelsDirectory + "/" + kDirName + "/bin/sherpa-onnx-offline";
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
    if (m_downloading)
        return;
    const bool ready = QFileInfo::exists(m_dir + "/" + kDirName + "/int8-onnx/model.int8.onnx")
                       && !findBin(m_dir).isEmpty();
    if (ready || m_phase == "error")
        m_error.clear();
    setPhase(ready ? "ready" : "not-installed");
    m_bin = findBin(m_dir);
    if (ready)
        m_progress = 100;
}

void Engine::install() {
    if (m_downloading || m_phase == "ready")
        return;
    m_error.clear();
    m_progress = 0;
    setPhase("downloading");
    m_downloading = true;
    // --location follows the release redirect; --fail makes a 404 page exit
    // non-zero instead of being saved as a "model"; --continue-at resumes a
    // partial download after a network drop.
    m_fetch.start("curl", {"-fL", "--continue-at", "-", "--no-progress-meter",
                           "-o", m_dir + "/asr.tar.bz2", kArchiveUrl});
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
    m_progress = int(qBound<qint64>(1, got * 100 / kArchiveBytes, qint64(99)));
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
    QProcess tar;
    tar.setWorkingDirectory(m_dir);
    tar.start("tar", {"xjf", archive});
    if (!tar.waitForFinished(300000) || tar.exitCode() != 0) {
        tar.kill();
        QFile::remove(archive);
        m_error = tr("The downloaded model archive could not be extracted.");
        setPhase("error");
        return;
    }
    QFile::remove(archive);
    m_progress = 100;
    m_bin = findBin(m_dir);
    const bool ok = !m_bin.isEmpty()
                    && QFileInfo::exists(m_dir + "/" + kDirName + "/int8-onnx/model.int8.onnx");
    if (!ok) {
        m_error = tr("The model archive did not contain what vox expected.");
        setPhase("error");
        return;
    }
    setPhase("ready");
}

void Engine::remove() {
    QDir(m_dir + "/" + kDirName).removeRecursively();
    QFile::remove(m_dir + "/asr.tar.bz2");
    m_progress = 0;
    refresh();
}

// The CLI echoes the input filename before each result and prefixes the text
// with "Recognized text:" on some builds; be liberal in what we accept, and
// drop the echo lines rather than trusting a fixed line offset.
QString Engine::parseOutput(const QByteArray &raw) {
    const QStringList lines = QString::fromUtf8(raw).split('\n', Qt::SkipEmptyParts);
    QStringList kept;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.endsWith(".wav", Qt::CaseInsensitive))
            continue;
        for (const char *prefix : {"Recognized text:", "Text:"}) {
            const QString head = QString::fromLatin1(prefix);
            if (line.startsWith(head, Qt::CaseInsensitive)) {
                line = line.mid(head.size()).trimmed();
                break;
            }
        }
        if (!line.isEmpty())
            kept.append(line);
    }
    return kept.join(' ').trimmed();
}

void Engine::transcribe(const QString &wavPath, const QString &language) {
    if (m_phase != "ready" || m_bin.isEmpty()) {
        emit failed(tr("The speech model is not installed yet — press M in the window to download it once."));
        return;
    }
    if (m_busy)
        return;
    const QString root = m_dir + "/" + kDirName;
    QStringList args{
        "--sense-voice-model=" + root + "/int8-onnx/model.int8.onnx",
        "--tokens=" + root + "/tokens.txt",
        "--use-itn=1",           // SenseVoice punctuates and normalizes numbers
        "--language=" + (language.isEmpty() ? QStringLiteral("auto") : language),
        "--num-threads=" + QString::number(qMax(1, QThread::idealThreadCount() - 1)),
        wavPath,
    };
    m_worker.start(m_bin, args);
    if (!m_worker.waitForStarted(5000)) {
        emit failed(tr("The recognition engine could not start."));
        return;
    }
    m_busy = true;
    emit busyChanged();
    m_watchdog.start();
}
