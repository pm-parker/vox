#pragma once

#include <QLocalServer>
#include <QObject>
#include <QSet>

class Backend;

// Single-instance control socket: a second `duckvox` (or any script) sends one
// JSON command per line — hold-start, hold-stop, toggle, status — and reads
// one JSON line back. The socket lives at $XDG_RUNTIME_DIR/duckvox/control.sock
// and is removed on shutdown, so the Omarchy dictation adapter never has to
// guess whether the GUI is up: a client that finds no socket starts the app.
class ControlServer : public QObject {
    Q_OBJECT

public:
    explicit ControlServer(Backend *backend, QObject *parent = nullptr);
    ~ControlServer() override;

    bool listen();

signals:
    void quitRequested();
    void holdStarted();
    void holdStopped();
    void showRequested();

private:
    QByteArray handleLine(const QByteArray &line);
    QByteArray statusJson() const;

    Backend *m_backend;
    QLocalServer m_server;
    QSet<QLocalSocket *> m_clients;
};
