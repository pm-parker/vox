#include <QDateTime>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
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

// Rename-day migration (pre-2.0 installs): move ~/.local/share/vox to
// ~/.local/share/duckvox wholesale — models, settings, history, takes. A
// moved dir needs no copy; a partial copy is deleted so a later run retries.
void migrateLegacyDataDir() {
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QDir legacy(dataRoot + "/vox");
    if (legacy.exists()) {
        const QString target = dataRoot + "/duckvox";
        if (QDir(target).exists()) {
            // Both exist: duckvox owns the future. Take only what the old dir
            // has that the new one lacks, then retire the legacy dir.
            for (const QString &entry :
                 legacy.entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
                if (!QFileInfo::exists(target + "/" + entry))
                    QFile::rename(legacy.absolutePath() + "/" + entry, target + "/" + entry);
            QDir(dataRoot).rmdir("vox"); // rmdir: only succeeds when empty
        } else if (QDir(dataRoot).rename("vox", "duckvox")) {
            qInfo() << "duckvox: migrated data directory from ~/.local/share/vox";
        }
    }

    // Hotkey overrides and the start-at-login line move too. The adapter
    // script reads ~/.config/duckvox/shortcuts.lua, and autostart must
    // launch the new binary or the next login resurrects the old name.
    // GenericConfigLocation is ~/.config itself (AppConfigLocation would
    // nest under ~/.config/duckvox and never see hypr/).
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QDir legacyConfig(configRoot + "/vox");
    if (legacyConfig.exists())
        QDir(configRoot).rename("vox", "duckvox");
    const QString autostartFile = configRoot + "/hypr/autostart.lua";
    QFile autostart(autostartFile);
    if (autostart.open(QIODevice::ReadOnly)) {
        QString text = QString::fromUtf8(autostart.readAll());
        autostart.close();
        const QString updated = text.replace(QLatin1String("vox --minimized"),
                                             QLatin1String("duckvox --minimized"));
        if (updated != text && autostart.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            autostart.write(updated.toUtf8());
            autostart.close();
        }
    }

    // The bar plugin under the user's plugins dir keeps its id, but its
    // description/poll target changed name; the package's installer re-syncs
    // it on upgrade, so just drop a stale copy's config twin.
    const QDir legacyPanel(QDir::homePath() + "/.config/omarchy/plugins/vox-panel");
    if (legacyPanel.exists())
        QDir(legacyPanel.path()).removeRecursively();
}

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
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/duckvox/control.sock";
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

// --print-setting KEY reads ~/.local/share/duckvox/settings.json offline — the
// keys script runs it while duckvox may not be running, and must never spawn a
// daemon just to learn a key name. Shell-safe output: single value, no echo.
int printSetting(const QString &key) {
    QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + "/duckvox/settings.json");
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

// Omarchy loads the float rule for DuckVox from
// /usr/share/omarchy/default/hypr/apps/duckvox.lua, but Hyprland only reads
// that file on `hyprctl reload`. A freshly installed or upgraded package
// therefore leaves the panel tiled until the user reloads the compositor or
// logs out again — the window then opens as a half-height slit.
//
// Inject the same rule at runtime instead: Hyprland applies runtime rules to
// windows mapped later, and the QML window is created after this call, so the
// panel floats from the very first "show". Failures are ignored — outside
// Hyprland there is simply no rule to install.
void ensureFloatingWindowRule() {
    const int windowWidth = 440; // must match the QML window width
    int x = 24;
    int y = 96;

    // Mirror the config rule's placement: right edge, 16% down the screen.
    QProcess monitorQuery;
    monitorQuery.start(QStringLiteral("hyprctl"),
                       {QStringLiteral("monitors"), QStringLiteral("-j")});
    if (monitorQuery.waitForFinished(1500)) {
        const QJsonArray monitors = QJsonDocument::fromJson(monitorQuery.readAllStandardOutput()).array();
        for (const QJsonValue &value : monitors) {
            const QJsonObject monitor = value.toObject();
            if (!monitor.value(QStringLiteral("focused")).toBool())
                continue;
            x = monitor.value(QStringLiteral("x")).toInt()
                + monitor.value(QStringLiteral("width")).toInt() - windowWidth - 24;
            y = monitor.value(QStringLiteral("y")).toInt()
                + qRound(monitor.value(QStringLiteral("height")).toDouble() * 0.16);
            break;
        }
    }

    const QString rule =
        QStringLiteral("hl.window_rule({match={class=\"duckvox$\"}, float=true, move={%1,%2}})")
            .arg(x)
            .arg(y);
    QProcess ruleInjection;
    ruleInjection.start(QStringLiteral("hyprctl"), {QStringLiteral("eval"), rule});
    if (!ruleInjection.waitForFinished(1500))
        return;
    const QString result = QString::fromUtf8(ruleInjection.readAllStandardOutput()).trimmed();
    if (!result.isEmpty() && !result.startsWith(QLatin1String("ok")))
        qWarning() << "duckvox: floating window rule not installed:" << result;
}

int main(int argc, char *argv[]) {
    // CLI mode first, before QApplication grabs a display: duckvox --toggle
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
                // omarchy-dictation-duckvox-keys). Only the shortcuts subkeys.
                if (i + 1 >= argc) {
                    std::fprintf(stderr, "Usage: duckvox --print-setting toggleKey|pttKey\n");
                    return 2;
                }
                return printSetting(QString::fromUtf8(argv[i + 1]));
            }
            else if (arg == "--minimized") command = "__spawn"; // handled below
            else { std::fprintf(stderr, "Usage: duckvox [--toggle|--hold-start|--hold-stop|--status|--copy-last|--show|--minimized|--print-setting KEY]\n"); return 2; }
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
                std::fprintf(stderr, "duckvox is not running\n");
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
    QApplication::setApplicationName("duckvox");
    QApplication::setWindowIcon(QIcon(":/duckvox/icons/duck.svg"));
    migrateLegacyDataDir();

    const QString dataDir = QApplication::applicationDirPath() + "/data";
    auto *store = new Store({}, &app);
    auto *engine = new Engine({}, &app); // default model dir under ~/.local/share/duckvox
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
    // Defaults matter: empty strings make Theme/Translator read Omarchy's
    // LIVE state (~/.local/state/omarchy/current, ~/.config/omarchy/language).
    // They were pointed at dataDir/current/... which never exists, so the
    // language catalog silently resolved to English and the theme only worked
    // through its built-in fallback.
    auto *theme = new Theme({}, &app);
    auto *translator = new Translator({}, ":/duckvox/i18n", &app);
    // Actually install it: a QTranslator registered only as a QML singleton
    // never answers qsTr(), so every label silently stayed English no matter
    // what Omarchy's language file said. Later language changes retranslate
    // through the Translator::changed connection below (the catalog is already
    // re-read by then).
    app.installTranslator(translator);
    // Brand switch: settings wins over the theme. The store's default (true)
    // keeps every fresh install in duck yellow; turning it off returns the
    // accent to Omarchy's live theme.
    theme->setDuckYellow(store->duckYellow());
    QObject::connect(store, &Store::settingsChanged, theme, [store, theme] {
        theme->setDuckYellow(store->duckYellow());
    });
    // UI language: the Settings card writes Store::uiLanguage; the translator
    // applies it live (override beats Omarchy's language file, "auto" =
    // follow the desktop). Set before QML loads so the first paint is right.
    translator->setOverride(store->uiLanguage());
    QObject::connect(store, &Store::settingsChanged, translator, [store, translator] {
        translator->setOverride(store->uiLanguage());
    });
    qInfo() << "duckvox: collaborators wired";

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
    // Before the QML window is created: the float rule has to exist when the
    // window maps, otherwise Hyprland tiles it.
    ensureFloatingWindowRule();

    // Plain qmake + RESOURCES has no qmldir, so load the qrc URL directly
    // instead of loadFromModule (that path needs qt_add_qml_module).
    qml.load(QUrl("qrc:/duckvox/ui/Main.qml"));
    qInfo() << "duckvox: qml.load returned, rootObjects =" << qml.rootObjects().size();

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
    // quitting — duckvox stays a dictation daemon until Quit is chosen.
    auto *rootWindow = qobject_cast<QQuickWindow *>(qml.rootObjects().value(0));
    auto *tray = new TrayIcon(backend, rootWindow, &app);
    Q_UNUSED(tray)
    QObject::connect(rootWindow, &QQuickWindow::visibilityChanged, tray,
                     [tray](QWindow::Visibility visibility) {
                         tray->windowVisible(visibility != QWindow::Hidden);
                     });

    return QApplication::exec();
}
