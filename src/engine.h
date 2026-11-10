#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

// The recognition engine: SenseVoice (zh/en/ja/ko/yue) run through the
// official sherpa-onnx-offline binary, downloaded once on explicit request
// (~230 MB with byte-accurate progress) and cached under
// ~/.local/share/duckvox/models. SenseVoice punctuates and inverse-normalizes
// numbers natively, so one model replaces crostivoice's browser ASR *and*
// its separate punctuation model. Without the model duckvox still works — the
// user sees the state and the rule pipeline polishes whatever text exists.
// Homophone correction (macbert4csc) is roadmap: the stock binary cannot run
// it, and shipping dead UI for it would lie.
// A catalog entry: one downloadable speech model with the flags its
// sherpa-onnx-offline invocation needs. Verified layouts — the tarballs all
// unpack to <dirName>/ and carry the files listed here.
struct ModelSpec {
    QString id;          // stable name stored in settings ("sense-voice")
    QString name;        // human label for the picker
    QString detail;      // one-line description (languages, size class)
    QString archiveUrl;  // release tarball (bz2)
    QString dirName;     // directory the archive extracts to
    QString modelFile;   // the onnx weights inside dirName
    QString tokensFile;  // tokens.txt inside dirName
    qint64 bytes;        // approximate download size, for progress
    QString kind;        // "sense-voice" | "paraformer" | "transducer"
    QStringList flags;   // engine flags before the wav argument
};

class Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)     // not-installed|downloading|extracting|ready|error
    Q_PROPERTY(int progress READ progress NOTIFY phaseChanged)   // 0..100 of the download
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)           // a transcription is running
    Q_PROPERTY(QString error READ error NOTIFY phaseChanged)
    // The model picked in settings (Store::model); switching re-stats the
    // cache and starts the picker's download on request.
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY phaseChanged)
    // Picker data: [[id, name, detail, installed], ...] — re-evaluated on
    // every phase change (a finished download flips `installed`).
    Q_PROPERTY(QVariantList models READ models NOTIFY phaseChanged)

public:
    explicit Engine(const QString &modelsDirectory = {}, QObject *parent = nullptr);
    ~Engine() override;

    QString phase() const { return m_phase; }
    int progress() const { return m_progress; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QString model() const { return m_model; }
    QVariantList models() const { return catalog(); }

    void refresh();            // stat the cache, set phase, never network

    Q_INVOKABLE void install();  // download + extract the selected model
    Q_INVOKABLE void remove();   // delete the cached model directory
    // The picker's data: [[id, name, detail, installed], ...]
    Q_INVOKABLE QVariantList catalog() const;

    static const QVector<ModelSpec> &catalogSpecs();
    static QString parseOutput(const QByteArray &raw);   // testable output cleaner

    // Runs the wav through the selected model. Emits transcribed(text) —
    // already punctuated where the model does it — or failed(message).
    void transcribe(const QString &wavPath, const QString &language, bool useItn = true);

public slots:
    void setModel(const QString &model);

signals:
    void phaseChanged();
    void busyChanged();
    void transcribed(const QString &text);
    void failed(const QString &message);

private:
    const ModelSpec &spec() const;               // the selected entry
    static QString findBin(const QString &modelsDirectory);
    void setPhase(const QString &phase);
    void finishDownload();
    void pollProgress();

    QString m_dir;
    QString m_bin;
    QString m_model = "sense-voice";
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
