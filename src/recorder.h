#pragma once

#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QProcess>
#include <QTimer>

// Microphone capture over PipeWire (parec raw s16le mono 16 kHz — exactly the
// format SenseVoice wants) with crostivoice's field-tested endpointing clocks:
// a generous ceiling before the first word, a silence gap that ends the take,
// and an absolute cap so a wedged microphone never records forever. Tests
// inject PCM through feed() without any process or device.
class Recorder : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(float level READ level NOTIFY levelChanged)       // 0..1 meter
    Q_PROPERTY(int elapsedMs READ elapsedMs NOTIFY tick)

public:
    explicit Recorder(const QString &takesDirectory = {}, QObject *parent = nullptr);

    bool recording() const { return m_recording; }
    float level() const { return m_level; }
    int elapsedMs() const { return m_recording ? int(m_clock.elapsed()) : m_lastElapsedMs; }
    QString takePath() const { return m_takePath; }

    // External feed mode lets tests (and any future in-process capture) drive
    // the endpointing state machine without spawning parec.
    void startExternalFeed(int gapMs, int waitFirstMs, int maxMs, bool autoStop);
    bool start(int gapMs, int waitFirstMs, int maxMs, bool autoStop, float vadThreshold);
    void stop(const QString &reason);
    void feed(const QByteArray &pcm);

    static bool writeWavHeader(QFile *file, int sampleRate, int channels, qint64 dataBytes);

signals:
    void recordingChanged();
    void levelChanged();
    void tick();
    void stopped(const QString &wavPath, const QString &reason);
    void failed(const QString &message);

private:
    void evaluateClocks();
    void finish(const QString &reason, const QString &error = {});

    QString m_takesDirectory;
    QProcess m_capture;
    QFile m_take;
    QString m_takePath;
    QElapsedTimer m_clock;
    QTimer m_clockTick;
    bool m_recording = false;
    bool m_autoStop = true;
    bool m_hasSpoken = false;
    int m_gapMs = 1600;
    int m_waitFirstMs = 9000;
    int m_maxMs = 120000;
    qint64 m_lastVoiceAt = 0;
    qint64 m_lastElapsedMs = 0;
    float m_level = 0;
    float m_threshold = 0.012f;
    qint64 m_dataBytes = 0;
    int m_pendingSamples = 0;
    qint64 m_windowEnergy = 0;
};
