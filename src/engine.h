#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>

// The recognition engine: SenseVoice (zh/en/ja/ko/yue) run through the
// official sherpa-onnx-offline binary, downloaded once on explicit request
// (~230 MB with byte-accurate progress) and cached under
// ~/.local/share/vox/models. SenseVoice punctuates and inverse-normalizes
// numbers natively, so one model replaces crostivoice's browser ASR *and*
// its separate punctuation model. Without the model vox still works — the
// user sees the state and the rule pipeline polishes whatever text exists.
// Homophone correction (macbert4csc) is roadmap: the stock binary cannot run
// it, and shipping dead UI for it would lie.
class Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)     // not-installed|downloading|extracting|ready|error
    Q_PROPERTY(int progress READ progress NOTIFY phaseChanged)   // 0..100 of the download
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)           // a transcription is running
    Q_PROPERTY(QString error READ error NOTIFY phaseChanged)

public:
    explicit Engine(const QString &modelsDirectory = {}, QObject *parent = nullptr);
    ~Engine() override;

    QString phase() const { return m_phase; }
    int progress() const { return m_progress; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }

    void refresh();            // stat the cache, set phase, never network

    Q_INVOKABLE void install();  // download + extract SenseVoice, on request
    Q_INVOKABLE void remove();   // delete the cached model and binary

    // Runs the wav through SenseVoice. Emits transcribed(text) — already
    // punctuated by the model — or failed(message).
    void transcribe(const QString &wavPath, const QString &language, bool useItn = true);

    static QString findBin(const QString &modelsDirectory);
    static QString parseOutput(const QByteArray &raw);   // testable output cleaner

signals:
    void phaseChanged();
    void busyChanged();
    void transcribed(const QString &text);
    void failed(const QString &message);

private:
    void setPhase(const QString &phase);
    void finishDownload();
    void pollProgress();

    QString m_dir;
    QString m_bin;
    QString m_phase = "not-installed";
    QString m_error;
    int m_progress = 0;
    bool m_busy = false;
    bool m_downloading = false;
    QProcess m_fetch;
    QProcess m_worker;
    QTimer m_progressTimer;
    QTimer m_watchdog;
};
