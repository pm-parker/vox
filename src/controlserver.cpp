#include "controlserver.h"

#include "backend.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QStandardPaths>

namespace {
QString socketPath() {
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return runtime + "/vox/control.sock";
}
} // namespace

ControlServer::ControlServer(Backend *backend, QObject *parent)
    : QObject(parent), m_backend(backend) {
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *client = m_server.nextPendingConnection()) {
            m_clients.insert(client);
            connect(client, &QLocalSocket::disconnected, this, [this, client] {
                m_clients.remove(client);
                client->deleteLater();
            });
            connect(client, &QLocalSocket::readyRead, this, [this, client] {
                while (client->canReadLine()) {
                    const QByteArray reply = handleLine(client->readLine().trimmed());
                    client->write(reply + '\n');
                    if (reply.contains("\"quit\""))
                        emit quitRequested();
                }
            });
        }
    });
}

ControlServer::~ControlServer() {
    if (m_server.isListening())
        QFile::remove(socketPath());
}

bool ControlServer::listen() {
    const QString path = socketPath();
    QFile::remove(path); // a stale socket from a crash is not a running app
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/vox");
    if (!m_server.listen(path)) {
        qWarning() << "vox: control socket unavailable:" << m_server.errorString();
        return false;
    }
    return true;
}

QByteArray ControlServer::handleLine(const QByteArray &line) {
    if (line.isEmpty())
        return "{}";
    const QJsonObject request = QJsonDocument::fromJson(line).object();
    const QString command = request.value("command").toString();

    if (command == "status")
        return statusJson();

    if (command == "toggle") {
        m_backend->toggle();
        return statusJson();
    }

    if (command == "hold-start") {
        m_backend->holdStart();
        emit holdStarted();
        return statusJson();
    }

    if (command == "hold-stop") {
        m_backend->holdStop();
        emit holdStopped();
        return statusJson();
    }

    if (command == "copy-last") {
        m_backend->copyLast();
        return statusJson();
    }

    if (command == "show") {
        emit showRequested();
        return statusJson();
    }

    if (command == "quit") {
        return QByteArrayLiteral("{\"ok\":true,\"quit\":true}");
    }

    return QByteArrayLiteral("{\"error\":\"unknown command\"}");
}

QByteArray ControlServer::statusJson() const {
    QJsonObject object;
    object.insert("version", 1);
    object.insert("state", m_backend->state());
    object.insert("lastText", m_backend->lastText());
    object.insert("error", m_backend->errorText());
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
