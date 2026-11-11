// Offscreen screenshot generator for the README.
//
// Renders the real window (qrc:/duckvox/ui/Main.qml with the real Store, Engine,
// Recorder, Inserter, Backend, Theme and Translator that main.cpp wires) under
// the offscreen platform, then grabs pixel frames of each state. Speech is
// real: a PATH shim puts a fake `parec` first for this process only, and that
// shim streams a genuine 16 kHz mono voice recording into the Recorder's
// normal capture path, so VAD endpointing, the take WAV, sherpa-onnx
// recognition, TextPipeline and history all run for real.
//
//   ./screenshot_window <outdir> <speech.wav> <pcmDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QSize>
#include <QTimer>
#include <QWindow>

#include <functional>
#include <iostream>

#include "../src/backend.h"
#include "../src/engine.h"
#include "../src/inserter.h"
#include "../src/recorder.h"
#include "../src/store.h"
#include "../src/theme.h"
#include "../src/translator.h"

namespace {

constexpr int kSettle = 700;      // let a frame paint before grabbing it
constexpr int kTranscribe = 8000; // sherpa-onnx warm-up + decode budget

QString g_out;
QQmlApplicationEngine *g_qml = nullptr;

// grabWindow() returns the rendered frame under the offscreen platform.
void shoot(const QString &name) {
    auto *window = qobject_cast<QQuickWindow *>(g_qml->rootObjects().value(0));
    if (!window) {
        std::cerr << "screenshot: no window for " << qPrintable(name) << '\n';
        return;
    }
    const QImage frame = window->grabWindow();
    const QString path = g_out + "/" + name + ".png";
    if (frame.isNull() || !frame.save(path)) {
        std::cerr << "screenshot: failed to save " << qPrintable(path) << '\n';
        return;
    }
    std::cout << "wrote " << qPrintable(path) << " (" << frame.width() << 'x' << frame.height() << ")\n";
}

void runSteps(const std::function<void(int)> &step);

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "usage: screenshot_window <outdir> <speech.wav> <pcmDir>\n";
        return 2;
    }
    g_out = QString::fromLocal8Bit(argv[1]);

    // The fake parec only exists inside this process's environment. It reads
    // the WAV to speak from VOX_SCREENSHOT_SPEECH, so the Recorder's own
    // parec argument list stays untouched.
    const QString pcmDir = QString::fromLocal8Bit(argv[3]);
    qputenv("PATH", pcmDir.toLocal8Bit() + ':' + qgetenv("PATH"));
    qputenv("VOX_SCREENSHOT_SPEECH", QByteArray(argv[2]));

    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("omarchy");
    QGuiApplication::setApplicationName("vox");

    // Point every collaborator at the throwaway profile so the user's real
    // settings and history are never touched.
    const QString dataDir = pcmDir + "/data";
    auto *store = new Store(dataDir, &app);
    auto *engine = new Engine(dataDir + "/models", &app);
    auto *recorder = new Recorder(dataDir + "/takes", &app);
    auto *inserter = new Inserter(&app);
    auto *backend = new Backend(store, engine, recorder, inserter, &app);
    // Theme dir is optional (argv[4]) so the tool can render an arbitrary
    // theme instead of the live one; empty = the real Omarchy state dir.
    const QString themeDir = argc > 4 ? QString::fromLocal8Bit(argv[4]) : QString();
    // Optional "WxH" (argv[5]) reproduces the live window geometry: the tiler
    // clamps this window on small screens, and the layout must survive it.
    QSize forced;
    if (argc > 5) {
        const QStringList wh = QString::fromLocal8Bit(argv[5]).split('x');
        if (wh.size() == 2)
            forced = QSize(wh[0].toInt(), wh[1].toInt());
    }
    auto *theme = new Theme(themeDir, &app);
    auto *translator = new Translator({}, ":/i18n", &app);
    // Same as the daemon: without installTranslator the catalog never
    // answers qsTr() and every README shot renders English regardless of
    // the desktop language.
    app.installTranslator(translator);

    qmlRegisterSingletonInstance("Vox", 1, 0, "Backend", backend);
    qmlRegisterSingletonInstance("Vox", 1, 0, "Settings", store);
    qmlRegisterSingletonInstance("Vox", 1, 0, "Speech", engine);
    qmlRegisterSingletonInstance("Vox", 1, 0, "Theme", theme);
    qmlRegisterSingletonInstance("Vox", 1, 0, "I18n", translator);

    QQmlApplicationEngine qml;
    g_qml = &qml;
    QObject::connect(&qml, &QQmlApplicationEngine::warnings, &app, [](const QList<QQmlError> &warnings) {
        for (const QQmlError &w : warnings)
            qWarning().noquote() << "qml:" << w.toString();
    });
    QObject::connect(
        &qml, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QGuiApplication::exit(1); },
        Qt::QueuedConnection);
    qml.load(QUrl("qrc:/duckvox/ui/Main.qml"));
    if (qml.rootObjects().isEmpty())
        return 1;
    if (forced.isValid()) {
        if (auto *w = qobject_cast<QQuickWindow *>(qml.rootObjects().value(0)))
            w->resize(forced);
    }

    // idle -> listening -> processing -> idle, grabbing a frame in each.
    runSteps([&](int step) {
        switch (step) {
        case 0:
            shoot("01-idle");
            backend->toggle(); // starts the shimmed capture
            break;
        case 1:
            shoot("02-listening");
            break;
        case 2:
            shoot("03-processing");
            break;
        case 3:
            shoot("04-transcribed");
            std::cout << "last text: " << qPrintable(backend->lastText()) << '\n';
            break;
        case 4:
            // Flip to the Settings page (same property the tabs set) and
            // grab it: proves the settings surface renders its cards.
            if (auto *root = g_qml->rootObjects().value(0))
                root->setProperty("page", 1);
            break;
        case 5:
            shoot("05-settings");
            // Scroll to the bottom of the settings list: the cards below the
            // fold (model, shortcuts) must be reachable, not clipped away.
            if (auto *root = g_qml->rootObjects().value(0))
                root->setProperty("scrollToEnd", true);
            break;
        case 6:
            shoot("06-settings-bottom");
            if (auto *root = g_qml->rootObjects().value(0))
                root->setProperty("page", 2);
            break;
        case 7:
            shoot("07-history");
            QGuiApplication::quit();
            break;
        }
    });

    return QGuiApplication::exec();
}

namespace {

// Step cadence: paint, then act, then wait long enough for the real pipeline.
void runSteps(const std::function<void(int)> &step) {
    auto *index = new int(0);
    auto *fire = new QTimer(qApp);
    fire->setInterval(kSettle);
    QObject::connect(fire, &QTimer::timeout, qApp, [index, fire, step] {
        step(*index);
        ++*index;
        if (*index == 1)
            fire->setInterval(1200); // listening: catch the live meter
        else if (*index == 2)
            fire->setInterval(kSettle);
        else if (*index == 3)
            fire->setInterval(kTranscribe);
        else if (*index == 4)
            fire->setInterval(1200); // page flip repaint
    });
    // Give QML one frame to lay out before the first shot.
    QTimer::singleShot(400, fire, [fire] { fire->start(); });
}

} // namespace
