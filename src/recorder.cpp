#include "recorder.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
constexpr int kSampleRate = 16000;
constexpr int kChannels = 1;
constexpr int kWindowSamples = 320; // 20 ms RMS windows
}

Recorder::Recorder(const QString &takesDirectory, QObject *parent)
    : QObject(parent),
      m_takesDirectory(takesDirectory.isEmpty()
                           ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/vox/takes"
                           : takesDirectory) {
    QDir().mkpath(m_takesDirectory);
    m_clockTick.setInterval(100);
    connect(&m_clockTick, &QTimer::timeout, this, &Recorder::evaluateClocks);
    connect(&m_capture, &QProcess::readyReadStandardOutput, this, [this] {
        feed(m_capture.readAllStandardOutput());
    });
    connect(&m_capture, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        // The user-facing message names the tool, per Omarchy's "say what's
        // wrong" rule; parec comes from libpulse.
        finish("error", tr("Microphone capture failed: parec (libpulse) could not run."));
    });
}

void Recorder::startExternalFeed(int gapMs, int waitFirstMs, int maxMs, bool autoStop) {
    if (m_recording)
        return;
    m_gapMs = gapMs;
    m_waitFirstMs = waitFirstMs;
    m_maxMs = maxMs;
    m_autoStop = autoStop;
    m_hasSpoken = false;
    m_lastVoiceAt = 0;
    m_level = 0;
    m_dataBytes = 0;
    m_pendingSamples = 0;
    m_windowEnergy = 0;

    m_takePath = m_takesDirectory + "/take-" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".wav";
    m_take.setFileName(m_takePath);
    if (!m_take.open(QIODevice::ReadWrite)) {
        emit failed(tr("Cannot write take file: %1").arg(m_takePath));
        return;
    }
    writeWavHeader(&m_take, kSampleRate, kChannels, 0);

    m_recording = true;
    m_clock.restart();
    m_clockTick.start();
    emit recordingChanged();
}

bool Recorder::start(int gapMs, int waitFirstMs, int maxMs, bool autoStop, float vadThreshold,
                     const QString &source) {
    startExternalFeed(gapMs, waitFirstMs, maxMs, autoStop);
    if (!m_recording)
        return false;
    m_threshold = vadThreshold;
    // pare c raw stream: fixed format so the VAD math and the model input
    // agree on what the bytes mean. An empty source follows the system
    // default; a picked source is passed straight through.
    QStringList parecArgs{"--raw", "--format=s16", "--rate=16000", "--channels=1"};
    if (!source.isEmpty())
        parecArgs << ("--device=" + source);
    m_capture.start("parec", parecArgs);
    if (!m_capture.waitForStarted(3000)) {
        finish("error", tr("Microphone capture failed: install libpulse for parec."));
        return false;
    }
    return true;
}

void Recorder::feed(const QByteArray &pcm) {
    if (!m_recording)
        return;
    m_take.write(pcm);
    m_dataBytes += pcm.size();

    // RMS over 20 ms windows; a window straddling a read boundary keeps its
    // partial energy for the next one, so chunk sizes never skew the meter.
    const auto *samples = reinterpret_cast<const qint16 *>(pcm.constData());
    const int total = int(pcm.size()) / 2;
    for (int i = 0; i < total; ++i) {
        m_windowEnergy += qint64(samples[i]) * samples[i];
        if (++m_pendingSamples >= kWindowSamples) {
            const float rms = std::sqrt(float(m_windowEnergy) / (kWindowSamples * 32768.0f * 32768.0f));
            const float level = qBound(0.0f, rms * 8.0f, 1.0f);
            if (qAbs(level - m_level) > 0.02f) {
                m_level = level;
                emit levelChanged();
            }
            // Speech is any window over the threshold; the endpointer only
            // ever *ends* takes, so a noisy room at worst shortens the gap,
            // it cannot corrupt a take.
            if (rms >= m_threshold) {
                m_hasSpoken = true;
                m_lastVoiceAt = m_clock.elapsed();
            }
            m_windowEnergy = 0;
            m_pendingSamples = 0;
        }
    }
    emit tick();
}

void Recorder::evaluateClocks() {
    if (!m_recording)
        return;
    emit tick();
    const qint64 now = m_clock.elapsed();
    if (now > m_maxMs) {
        stop("max");
    } else if (!m_hasSpoken && now > m_waitFirstMs) {
        // Nothing transcribed and nothing spoken: give up quietly rather
        // than feeding the recognizer (and the user) silence.
        stop("silence");
    } else if (m_autoStop && m_hasSpoken && now - m_lastVoiceAt > m_gapMs) {
        // crostivoice's "说完停顿自动上屏" boundary: GAP_MS of silence after
        // the last speech ends the take. No grace window is needed here, as
        // it was for the streaming browser recognizer, because the take file
        // already holds the trailing silence.
        stop("gap");
    }
}

void Recorder::stop(const QString &reason) {
    if (!m_recording)
        return;
    if (m_capture.state() != QProcess::NotRunning) {
        m_capture.disconnect(this);
        m_capture.kill();
        m_capture.waitForFinished(1000);
    }
    finish(reason);
}

void Recorder::finish(const QString &reason, const QString &error) {
    m_clockTick.stop();
    m_lastElapsedMs = int(m_clock.elapsed());
    if (m_take.isOpen()) {
        // Patch the RIFF sizes now that the data length is known.
        m_take.seek(0);
        writeWavHeader(&m_take, kSampleRate, kChannels, m_dataBytes);
        m_take.close();
    }
    m_recording = false;
    m_level = 0;
    emit levelChanged();
    emit recordingChanged();
    if (!error.isEmpty())
        emit failed(error);
    else
        emit stopped(m_takePath, reason);
}

// Canonical 44-byte PCM WAV header, little endian throughout.
bool Recorder::writeWavHeader(QFile *file, int sampleRate, int channels, qint64 dataBytes) {
    if (!file->isWritable())
        return false;
    QByteArray header(44, '\0');
    auto put32 = [&](int offset, quint32 value) {
        for (int i = 0; i < 4; ++i)
            header[offset + i] = char((value >> (8 * i)) & 0xFF);
    };
    auto put16 = [&](int offset, quint16 value) {
        for (int i = 0; i < 2; ++i)
            header[offset + i] = char((value >> (8 * i)) & 0xFF);
    };
    header.replace(0, 4, "RIFF");
    put32(4, quint32(36 + dataBytes));
    header.replace(8, 4, "WAVE");
    header.replace(12, 4, "fmt ");
    put32(16, 16);                                   // fmt chunk size
    put16(20, 1);                                    // PCM
    put16(22, quint16(channels));
    put32(24, quint32(sampleRate));
    put32(28, quint32(sampleRate * channels * 2));   // byte rate
    put16(32, quint16(channels * 2));                // block align
    put16(34, 16);                                   // bits per sample
    header.replace(36, 4, "data");
    put32(40, quint32(dataBytes));
    file->seek(0);
    return file->write(header) == 44;
}
