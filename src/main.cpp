#include <QDateTime>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

#include <cstdio>

#include "backend.h"
#include "controlserver.h"
#include "engine.h"
#include "inserter.h"
#include "recorder.h"
#include "statusreporter.h"
#include "store.h"
#include "theme.h"
#include "translator.h"
#include "trayicon.h"

// One window, one job. Omarchy's own apps wire the same collaborators:
// Store (settings + history), Engine (SenseVoice), Recorder (parec),
// Inserter (wtype/wl-copy), Theme (live Omarchy accent) and Translator
// (live Omarchy language). QML renders Backend's state machine.
//
// Single instance: a second launch forwards its --command to the running
// daemon's control socket and exits — that is how the Omarchy dictation
// adapter drives push-to-talk without stealing focus.
namespace {
QString socketPath() {
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/vox/control.sock";
}

bool forwardToRunning(const QString &command) {
    QLocalSocket socket;
    socket.connectToServer(socketPath());
    if (!socket.waitForConnected(300))
        return false;
    socket.write(QJsonDocument(QJsonObject{{"command", command}}).toJson(QJsonDocument::Compact) + '\n');
    socket.waitForBytesWritten(300);
    socket.waitForReadyRead(500);
    std::printf("%s\n", socket.readAll().constData());
    socket.disconnectFromServer();
    return true;
}

// --print-setting KEY reads ~/.local/share/vox/settings.json offline — the
// keys script runs it while vox may not be running, and must never spawn a
// daemon just to learn a key name. Shell-safe output: single value, no echo.
int printSetting(const QString &key) {
    QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + "/vox/settings.json");
    // "shortcuts.pttKey" means the pttKey member of the shortcuts object;
    // everything else is a top-level settings key.
    const QString leaf = key.section('.', -1);
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject settings = QJsonDocument::fromJson(file.readAll()).object();
        const QJsonValue shortcuts = settings.value("shortcuts");
        if (shortcuts.isObject()) {
            const QString value = shortcuts.toObject().value(leaf).toString();
            if (!value.isEmpty()) {
                std::printf("%s\n", value.toUtf8().constData());
                return 0;
            }
        }
    }
    // Defaults must match the Shortcuts card's reset values.
    if (leaf == "pttKey")
        std::printf("ALT + Alt_R\n");
    else
        std::printf("ALT + SPACE\n");
    return 0;
}
} // namespace

int main(int argc, char *argv[]) {
    // CLI mode first, before QApplication grabs a display: vox --toggle
    // --hold-start --hold-stop --status --show forward to the running daemon.
    if (argc > 1 && argv[1][0] == '-') {
        QString command;
        for (int i = 1; i < argc; ++i) {
            const QString arg = QString::fromUtf8(argv[i]);
            if (arg == "--toggle") command = "toggle";
            else if (arg == "--hold-start") command = "hold-start";
            else if (arg == "--hold-stop") command = "hold-stop";
            else if (arg == "--status") command = "status";
            else if (arg == "--copy-last") command = "copy-last";
            else if (arg == "--show") command = "show";
            else if (arg == "--print-setting") {
                // Offline: read settings.json and print one value (used by
                // omarchy-dictation-vox-keys). Only the shortcuts subkeys.
                if (i + 1 >= argc) {
                    std::fprintf(stderr, "Usage: vox --print-setting toggleKey|pttKey\n");
                    return 2;
                }
                return printSetting(QString::fromUtf8(argv[i + 1]));
            }
            else if (arg == "--minimized") command = "__spawn"; // handled below
            else { std::fprintf(stderr, "Usage: vox [--toggle|--hold-start|--hold-stop|--status|--copy-last|--show|--minimized|--print-setting KEY]\n"); return 2; }
        }
        if (command == "__spawn") {
            // --minimized only makes sense as the daemon's own flag; fall
            // through to normal startup with the window hidden — unless a
            // daemon already runs: autostart double-fires and stale session
            // leftovers must surface the window, not fight over the socket.
            if (forwardToRunning("status")) {
                forwardToRunning("show");
                return 0;
            }
        } else {
            if (!forwardToRunning(command)) {
                std::fprintf(stderr, "vox is not running\n");
                return 1;
            }
            return 0;
        }
    } else if (forwardToRunning("status")) {
        // Plain second launch: the daemon is already up — surface its window
        // instead of fighting over the socket and the microphone.
        forwardToRunning("show");
        return 0;
    }

    bool minimized = false;
    for (int i = 1; i < argc; ++i)
        if (QString::fromUtf8(argv[i]) == "--minimized")
            minimized = true;

    QApplication app(argc, argv);
    QApplication::setOrganizationName("omarchy");
    QApplication::setApplicationName("vox");

    const QString dataDir = QApplication::applicationDirPath() + "/data";
    auto *store = new Store({}, &app);
    auto *engine = new Engine({}, &app); // default model dir under ~/.local/share/vox
    auto *recorder = new Recorder({}, &app);
    // Wake the microphone before the first hotkey: PipeWire sources suspend
    // when idle and swallow everything said while waking up.
    recorder->warmUp(store->microphone());
    auto *inserter = new Inserter(&app);
    auto *backend = new Backend(store, engine, recorder, inserter, &app);
    // Autostart list is synced once on boot (deferred so the file exists
    // even on a cold first start) and on every setting change.
    QTimer::singleShot(0, backend, &Backend::applyAutostart);
    QObject::connect(backend, &Backend::settingsChanged, backend, &Backend::applyAutostart);
    auto *reporter = new StatusReporter(&app);
    auto *control = new ControlServer(backend, &app);
    QObject::connect(backend, &Backend::stateChanged, reporter, [reporter, backend] {
        reporter->setState(backend->state());
    });
    QObject::connect(backend, &Backend::levelChanged, reporter, [reporter, backend] {
        reporter->setLevel(backend->level());
    });
    QObject::connect(backend, &Backend::lastTextChanged, reporter, [reporter, backend] {
        reporter->setLast(backend->lastText(), QDateTime::currentSecsSinceEpoch());
    });
    QObject::connect(backend, &Backend::errorChanged, reporter, [reporter, backend] {
        reporter->setError(backend->errorText());
    });
    QObject::connect(control, &ControlServer::quitRequested, &app, &QApplication::quit);
    QObject::connect(&app, &QApplication::aboutToQuit, reporter, [reporter] { reporter->setOffline(); });
    control->listen();
    auto *theme = new Theme(dataDir + "/current/theme", &app);
    auto *translator = new Translator(dataDir + "/current/language", ":/i18n", &app);
    qInfo() << "vox: collaborators wired";

    qmlRegisterSingletonInstance("Vox", 1, 0, "Backend", backend);
    qmlRegisterSingletonInstance("Vox", 1, 0, "Settings", store);
    qmlRegisterSingletonInstance("Vox", 1, 0, "Speech", engine);
    qmlRegisterSingletonInstance("Vox", 1, 0, "Theme", theme);
    qmlRegisterSingletonInstance("Vox", 1, 0, "I18n", translator);

    QQmlApplicationEngine qml;
    // Loud QML failures: creation warnings print to stderr before exit(1).
    QObject::connect(&qml, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
                         for (const QQmlError &w : warnings)
                             qWarning().noquote() << w.toString();
                     });
    // Live retranslation when Omarchy's language changes (Translator watches
    // the current-language file).
    QObject::connect(translator, &Translator::changed, &qml, &QQmlApplicationEngine::retranslate);
    QObject::connect(
        &qml, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QApplication::exit(1); }, Qt::QueuedConnection);
    // Plain qmake + RESOURCES has no qmldir, so load the qrc URL directly
    // instead of loadFromModule (that path needs qt_add_qml_module).
    qml.load(QUrl("qrc:/vox/ui/Main.qml"));
    qInfo() << "vox: qml.load returned, rootObjects =" << qml.rootObjects().size();

    if (qml.rootObjects().isEmpty())
        return 1;

    // "show" from the control socket (a plain second launch, or the bar
    // panel) surfaces the window; --minimized keeps it hidden at startup.
    QObject::connect(control, &ControlServer::showRequested, &app, [&qml] {
        if (auto *window = qobject_cast<QQuickWindow *>(qml.rootObjects().value(0))) {
            window->show();
            window->raise();
            window->requestActivate();
        }
    });

    if (minimized) {
        // Daemon mode for autostart and the dictation adapter: the window
        // exists but stays hidden until "show" arrives.
        if (auto *rootWindow = qobject_cast<QQuickWindow *>(qml.rootObjects().first()))
            rootWindow->hide();
    }

    // Native tray icon: the mic follows the dictation state, the menu drives
    // dictate/copy, and closing the window hides to the tray instead of
    // quitting — vox stays a dictation daemon until Quit is chosen.
    auto *rootWindow = qobject_cast<QQuickWindow *>(qml.rootObjects().value(0));
    auto *tray = new TrayIcon(backend, rootWindow, &app);
    Q_UNUSED(tray)
    QObject::connect(rootWindow, &QQuickWindow::visibilityChanged, tray,
                     [tray](QWindow::Visibility visibility) {
                         tray->windowVisible(visibility != QWindow::Hidden);
                     });

    return QApplication::exec();
}
